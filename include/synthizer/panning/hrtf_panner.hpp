#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <tuple>

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/data/hrtf.hpp"
#include "synthizer/math.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * HRTF implementation.
 *
 * This involves a variety of components which interact with one another to provide efficient HRTF processing of audio
 * data:
 *
 * - Low-level functions, which compute HRIRs and interaural time delay.
 * - A higher level HrtfPanner, to actually drive the HRTF.
 *
 * NOTE: because of established conventions for HRIR data that we didn't set, angles here are in degrees in order to
 * match our data sources. Specifically azimuth is clockwise of forward and elevation ranges from -90 to 90.
 *
 * Also the dataset is asumed to be for the left ear. Getting this backward will cause the impulses to be flipped.
 * */

/*
 * Parameters for an Hrir model.
 *
 * All units are SI.
 * */
struct HrirParameters {
  /* head_radius is a guess. TODO: figure out a better way to compute this number. */
  double head_radius = 0.08;
  double speed_of_sound = 343;
};

/*
 * Compute the interaural time difference for a source.
 *
 * Returns a tuple {l, r} where one of l or r is always 0.
 *
 * Unit is fractional samples.
 * */
std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation);
std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation,
                                                           const HrirParameters *hrir_parameters);

/*
 * Read the built-in HRIR dataset, interpolate as necessary, and fill the output buffers with impulses for convolution.
 * */
void computeHrtfImpulses(double azimuth, double elevation, float *left, unsigned int left_stride, float *right,
                         unsigned int right_stride);

/*
 * An HRTF convolver with ITD>
 * */
class HrtfPanner {
public:
  HrtfPanner() : moved(true) {}

  unsigned int getOutputChannelCount();
  float *getInputBuffer();
  void run(float *output);
  void setPanningAngles(double azimuth, double elevation);
  void setPanningScalar(double scalar);

private:
  template <typename MP> void stepConvolution(MP &&ptr, const float *hrir, float *out_l, float *out_r);

  /*
   * 2 blocks plus the max ITD.
   * We'll use modulus on 1/3 blocks on average.
   * */
  BlockDelayLine<1, nextPowerOfTwo(config::HRTF_MAX_ITD + config::BLOCK_SIZE * 2) / config::BLOCK_SIZE> input_line;

  /* This is an extra sample long to make linear interpolation easier. */
  BlockDelayLine<2, nextPowerOfTwo(config::BLOCK_SIZE * 2 + config::HRTF_MAX_ITD + 1) / config::BLOCK_SIZE> itd_line;

  /*
   * The hrirs and an index which determines which we are using.
   *
   * The way this works is that current_hrir is easily flipped with xor. current_hrir ^ 1 is the previous, (current_hrir
   * ^ 1)*2*data::hrtf::RESPONSE_LENGTH is the index of the previous.
   * */
  std::array<float, data::hrtf::IMPULSE_LENGTH * 2 * 2> hrirs = {0.0f};
  unsigned int current_hrir = 0;
  float prev_itd_l = 0.0, prev_itd_r = 0.0;

  double azimuth = 0.0;
  double elevation = 0.0;
  bool moved = false;
};

inline const HrirParameters DEFAULT_HRIR_PARAMETERS{};

inline std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation) {
  return computeInterauralTimeDifference(azimuth, elevation, &DEFAULT_HRIR_PARAMETERS);
}

inline std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation,
                                                                  const HrirParameters *hrir_parameters) {
  double az_r = azimuth * PI / 180;
  double elev_r = elevation * PI / 180;

  /*
   * Convert the polar vector to a cartesian vector, and keep x.
   *
   * az_r is the clockwise angle from y.
   * cos(az_r) is the y component.
   * Consequently sin(az_r) is the x component and cos(az_r) is y.
   *
   * This is the standard spherical coordinate equation, but adjusted to account for the above.
   * */
  double x = sin(az_r) * cos(elev_r);
  /*
   * Get the angle between the y axis and our spherical vector.
   *
   * It is convenient for this to be in the range 0, PI/2, which "pretends" the source is in the front right quadrant.
   * Since the head is front-back symmetric, this is correct for the back right quadrant, and we work out whether to
   * flip at the end.
   *
   * What we have is the angle between the x axis and our spherical vector,
   * so to get y we subtract.
   * */
  double angle = PI / 2 - acos(fabs(x));

  /* Interaural time delay in seconds, using the Woodworth formula. */
  double itd_s = (hrir_parameters->head_radius / hrir_parameters->speed_of_sound) * (angle + sin(angle));
  double itd = std::min<double>(itd_s * config::SR, config::HRTF_MAX_ITD);

  /*
   * Work out if this is for the right or left ear.
   *
   * Elevation can never cause the source to swap sides of the head, so we uze az_r as is.
   *
   * Additionally input angles to this function are never negative, so this can be accomplished by taking (int)az_r/pi
   * and determining if it is odd or not. This works because for 0 <= az_r <= pi, 2pi <= az_r <= 3pi, etc. the source is
   * on the right side of the head.
   * */
  unsigned int intervals = az_r / PI;
  bool left = intervals % 2 == 1;
  /* This looks backwards but isn't: if the source is to the left, the right ear is delayed. */
  if (left)
    return {0, itd};
  else
    return {itd, 0};
}

/*
 * Helper to get weights for linear interpolation.
 * */
inline std::tuple<double, double> linearInterpolate(double val, double start, double end) {
  /* These need to be loose because of floating point error, but let's catch any weirdness beyond what's reasonable. */
  assert(val - start > -0.01);
  assert(end - val > -0.01);

  if (start == end)
    return {0.5, 0.5};
  /* Clamp to eliminate floating point error. */
  val = std::min(std::max(val, start), end);

  /* The right (upper) weight. */
  double w1 = (val - start) / (end - start);
  return {1 - w1, w1};
}

inline void computeHrtfImpulseSingleChannel(double azimuth, double elevation,
                                            const data::hrtf::ElevationDef *elev_lower,
                                            const data::hrtf::ElevationDef *elev_upper, float *out,
                                            unsigned int out_stride) {
  std::array<double, 4> weights = {0.0};
  std::array<const float *, 4> impulses = {nullptr};
  unsigned int weight_count = 0;

  std::array<const data::hrtf::ElevationDef *, 2> elevs = {elev_lower, elev_upper};
  std::array<double, 2> elev_weights{1.0, 1.0};

  if (elev_upper != nullptr) {
    /*
     * Not linear interpolation, bilinear interpolation.
     * It's necessary to interpolate a second time for the elevations.
     * */
    auto [e0, e1] = linearInterpolate(elevation, elev_lower->angle, elev_upper->angle);
    elev_weights[0] = e0;
    elev_weights[1] = e1;
  }

  for (unsigned int j = 0; j < elevs.size(); j++) {
    auto e = elevs[j];
    auto ew = elev_weights[j];

    if (e == nullptr)
      break;

    /*
     * The current HRTF implementation assumes that each azimuth is equidistant, so to compute the indices we can do
     * simple multiplication etc.
     *
     * i is the unmodulused angle, which makes math for linear interpolation easier; i1 and i2 let us get  the impulses
     * themselves.
     * */
    unsigned int i = (unsigned int)(azimuth / (360.0 / e->azimuth_count));
    unsigned int i1 = i % e->azimuth_count;
    unsigned int i2 = (i1 + 1) % e->azimuth_count;

    impulses[weight_count] = &data::hrtf::IMPULSES[e->azimuth_start + i1][0];
    impulses[weight_count + 1] = &data::hrtf::IMPULSES[e->azimuth_start + i2][0];

    if (i1 == i2) {
      /* Only one impulse. */
      weights[weight_count] = 1.0 * ew;
      weight_count += 1;
    } else {
      auto [w1, w2] = linearInterpolate(azimuth, i * (360.0 / e->azimuth_count), (i + 1) * (360.0 / e->azimuth_count));
      weights[weight_count] = w1 * ew;
      weights[weight_count + 1] = w2 * ew;
      weight_count += 2;
    }
  }

  for (unsigned int i = 0; i < data::hrtf::IMPULSE_LENGTH; i++) {
    out[i * out_stride] = impulses[0][i] * weights[0];
  }

  for (unsigned int c = 1; c < weight_count; c++) {
    float *cursor = out;
    for (unsigned int i = 0; i < data::hrtf::IMPULSE_LENGTH; i++, cursor += out_stride) {
      *cursor += impulses[c][i] * weights[c];
    }
  }
}

inline void computeHrtfImpulses(double azimuth, double elevation, float *left, unsigned int left_stride, float *right,
                                unsigned int right_stride) {
  const data::hrtf::ElevationDef *elev_lower = nullptr, *elev_upper = nullptr;

  assert(azimuth >= 0.0 && azimuth <= 360.0);
  assert(elevation >= -90.0 && elevation <= 90.0);

  for (auto &e : data::hrtf::ELEVATIONS) {
    if (e.angle <= elevation) {
      elev_lower = &e;
    } else {
      elev_upper = &e;
      break;
    }
  }

  /*
   * If this is -90, then elev_lower might be null.
   * If this is 90, then elev_upper might be null.
   * Fix them up so that neither is.
   */
  assert(elev_lower || elev_upper);
  elev_lower = elev_lower ? elev_lower : elev_upper;
  elev_upper = elev_upper ? elev_upper : elev_lower;
  /*
   * Elevation could be below elev_lower or above elev_upper, if the entire sphere isn't covered.
   * */
  elevation = clamp(elevation, elev_lower->angle, elev_upper->angle);

  computeHrtfImpulseSingleChannel(azimuth, elevation, elev_lower, elev_upper, left, left_stride);
  computeHrtfImpulseSingleChannel(360 - azimuth, elevation, elev_lower, elev_upper, right, right_stride);
}

inline unsigned int HrtfPanner::getOutputChannelCount() { return 2; }

inline float *HrtfPanner::getInputBuffer() { return this->input_line.getNextBlock(); }

template <typename MP>
inline void HrtfPanner::stepConvolution(MP &&ptr, const float *hrir, float *dest_l, float *dest_r) {
  float accumulator_left = 0.0f;
  float accumulator_right = 0.0f;
  for (unsigned int j = 0; j < data::hrtf::IMPULSE_LENGTH; j++) {
    float sample = *(ptr - j);
    float hrir_left = hrir[j * 2];
    float hrir_right = hrir[j * 2 + 1];

    accumulator_left += sample * hrir_left;
    accumulator_right += sample * hrir_right;
  }

  *dest_l = accumulator_left;
  *dest_r = accumulator_right;
}

inline void HrtfPanner::run(float *output) {
  float *prev_hrir = nullptr;
  float *cur_hrir = &this->hrirs[this->current_hrir * 2 * data::hrtf::IMPULSE_LENGTH];

  bool crossfade = this->moved;
  this->moved = false;

  if (crossfade) {
    prev_hrir = cur_hrir;
    this->current_hrir ^= 1;
    cur_hrir = &this->hrirs[this->current_hrir * 2 * data::hrtf::IMPULSE_LENGTH];
  }

  if (crossfade) {
    computeHrtfImpulses(this->azimuth, this->elevation, cur_hrir, 2, cur_hrir + 1, 2);
  }

  unsigned int crossfade_samples = crossfade ? config::CROSSFADE_SAMPLES : 0;
  unsigned int normal_samples = config::BLOCK_SIZE - crossfade_samples;

  float *itd_block = this->itd_line.getNextBlock();
  auto input_mp = this->input_line.getModPointer(data::hrtf::IMPULSE_LENGTH - 1);
  std::visit(
      [&](auto &ptr) {
        for (std::size_t i = 0; i < crossfade_samples; i++) {
          float l_old, l_new, r_old, r_new;
          this->stepConvolution(ptr, prev_hrir, &l_old, &r_old);
          this->stepConvolution(ptr, cur_hrir, &l_new, &r_new);
          float *out = itd_block + 2 * i;
          float w1 = i / (float)config::CROSSFADE_SAMPLES;
          float w0 = 1.0f - w1;

          out[0] = l_old * w0 + l_new * w1;
          out[1] = r_old * w0 + r_new * w1;

          ++ptr;
        }

        for (std::size_t i = crossfade_samples; i < config::BLOCK_SIZE; i++) {
          this->stepConvolution(ptr, cur_hrir, &itd_block[2 * i], &itd_block[2 * i + 1]);
          ++ptr;
        }
      },
      input_mp);
  this->input_line.incrementBlock();

  /*
   *pre-unrolled weights for the left and right ear.
   * Early is too little delay. Late is too much.
   */
  auto itds = computeInterauralTimeDifference(this->azimuth, this->elevation);
  float itd_l = std::get<0>(itds);
  float itd_r = std::get<1>(itds);
  unsigned int itd_l_i = itd_l, itd_r_i = itd_r;

  float itd_w_late_l = itd_l - itd_l_i;
  float itd_w_early_l = 1.0 - itd_w_late_l;
  float itd_w_late_r = itd_r - itd_r_i;
  float itd_w_early_r = 1.0 - itd_w_late_r;

  auto itd_mp = this->itd_line.getModPointer(config::HRTF_MAX_ITD);
  std::visit(
      [&](auto ptr) {
        for (std::size_t i = 0; i < crossfade_samples; i++) {
          float *o = output + i * 2;
          float fraction = i / (float)config::CROSSFADE_SAMPLES;

          float prev_itd_l = this->prev_itd_l, prev_itd_r = this->prev_itd_r;

          float left = itd_l * fraction + prev_itd_l * (1.0 - fraction);
          float right = itd_r * fraction + prev_itd_r * (1.0 - fraction);
          assert(left >= 0.0);
          assert(right >= 0.0);
          unsigned int left_s = left, right_s = right;
          float wl = left - left_s;
          float wr = right - right_s;
          auto left_ptr = ptr - 2 * (left_s + 1);
          auto right_ptr = ptr - 2 * (right_s + 1);
          float lse = left_ptr[2], lsl = left_ptr[0];
          float rse = right_ptr[3], rsl = right_ptr[1];
          float ls = lsl * wl + lse * (1.0f - wl);
          float rs = rsl * wr + rse * (1.0f - wr);
          o[0] += ls;
          o[1] += rs;
          ptr += 2;
        }

        for (std::size_t i = crossfade_samples; i < config::BLOCK_SIZE; i++) {
          float *o = output + i * 2;
          auto left_ptr = ptr - 2 * (itd_l_i + 1);
          auto right_ptr = ptr - 2 * (itd_r_i + 1);
          float lse = left_ptr[2], lsl = left_ptr[0];
          float rse = right_ptr[3], rsl = right_ptr[1];
          o[0] += itd_w_early_l * lse + itd_w_late_l * lsl;
          o[1] += itd_w_early_r * rse + itd_w_late_r * rsl;
          ptr += 2;
        }
      },
      itd_mp);
  this->itd_line.incrementBlock();

  this->prev_itd_l = itd_l;
  this->prev_itd_r = itd_r;
}

inline void HrtfPanner::setPanningAngles(double _azimuth, double _elevation) {
  this->moved = this->moved || this->azimuth != _azimuth || this->elevation != _elevation;
  this->azimuth = _azimuth;
  this->elevation = _elevation;
}

inline void HrtfPanner::setPanningScalar(double scalar) {
  assert(scalar >= -1.0 && scalar <= 1.0);
  if (scalar >= 0.0) {
    this->setPanningAngles(90 * scalar, 0.0);
  } else {
    this->setPanningAngles(360.0 + 90.0 * scalar, 0.0);
  }
}

} // namespace synthizer
