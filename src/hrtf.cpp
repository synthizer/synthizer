#include "synthizer/hrtf.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <tuple>

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/data/hrtf.hpp"
#include "synthizer/math.hpp"

/*
 * Another entirely math file, so bring math things into scope.
 * Keep non-cmath things qualified.
 * */
using std::sin, std::cos;

namespace synthizer {

static const HrirParameters DEFAULT_HRIR_PARAMETERS{};

std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation) {
  return computeInterauralTimeDifference(azimuth, elevation, &DEFAULT_HRIR_PARAMETERS);
}

std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation,
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
std::tuple<double, double> linearInterpolate(double val, double start, double end) {
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

void computeHrtfImpulseSingleChannel(double azimuth, double elevation, const data::hrtf::ElevationDef *elev_lower,
                                     const data::hrtf::ElevationDef *elev_upper, float *out, unsigned int out_stride) {
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

void computeHrtfImpulses(double azimuth, double elevation, float *left, unsigned int left_stride, float *right,
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

HrtfPanner::HrtfPanner() { this->moved.setAll(true); }

unsigned int HrtfPanner::getOutputChannelCount() { return 2; }

unsigned int HrtfPanner::getLaneCount() { return CHANNELS; }

std::tuple<float *, unsigned int> HrtfPanner::getLane(unsigned int channel) {
  assert(channel < HrtfPanner::CHANNELS);
  float *ptr = this->input_line.getNextBlock();
  return {ptr + channel, CHANNELS};
}

void HrtfPanner::releaseLane(unsigned int lane) {
  assert(lane < CHANNELS);

  this->input_line.clearChannel(lane);
}

template <typename R>
void HrtfPanner::stepConvolution(R &&reader, const float *hrir, std::array<float, 4> *dest_l,
                                 std::array<float, 4> *dest_r) {
  std::array<float, 4> accumulator_left = {{0.0f}};
  std::array<float, 4> accumulator_right = {{0.0f}};
  for (unsigned int j = 0; j < data::hrtf::IMPULSE_LENGTH; j++) {
    auto tmp = reader.readFrame(j);
    auto hrir_left = &hrir[j * 2 * 4];
    auto hrir_right = &hrir[(j * 2 + 1) * 4];
    for (unsigned int k = 0; k < CHANNELS; k++) {
      accumulator_left[k] += tmp[k] * hrir_left[k];
      accumulator_right[k] += tmp[k] * hrir_right[k];
    }
  }
  // We have to scatter these out, to re-interleave them.
  *dest_l = std::array<float, 4>{accumulator_left[0], accumulator_right[0], accumulator_left[1], accumulator_right[1]};
  *dest_r = std::array<float, 4>{accumulator_left[2], accumulator_right[2], accumulator_left[3], accumulator_right[3]};
}

void HrtfPanner::run(float *output) {
  float *prev_hrir = nullptr;
  float *cur_hrir = &this->hrirs[this->current_hrir * CHANNELS * 2 * data::hrtf::IMPULSE_LENGTH];

  bool crossfade = false;
  for (unsigned int i = 0; i < CHANNELS; i++) {
    crossfade |= this->moved.get(i);
    this->moved.set(i, false);
  }

  if (crossfade) {
    prev_hrir = cur_hrir;
    this->current_hrir ^= 1;
    cur_hrir = &this->hrirs[this->current_hrir * CHANNELS * 2 * data::hrtf::IMPULSE_LENGTH];
  }

  std::array<std::tuple<double, double>, CHANNELS> itds = this->prev_itds;

  if (crossfade) {
    for (unsigned int i = 0; i < CHANNELS; i++) {
      computeHrtfImpulses(this->azimuths[i], this->elevations[i], &cur_hrir[i], 8, &cur_hrir[i + 4], 8);
      itds[i] = computeInterauralTimeDifference(azimuths[i], elevations[i]);
    }
  }

  unsigned int crossfade_samples = crossfade ? config::CROSSFADE_SAMPLES : 0;
  unsigned int normal_samples = config::BLOCK_SIZE - crossfade_samples;
  assert(crossfade_samples + normal_samples == config::BLOCK_SIZE);

  float *itd_block = this->itd_line.getNextBlock();
  input_line.runReadLoopSplit(
      data::hrtf::IMPULSE_LENGTH - 1, crossfade_samples,
      [&](unsigned int i, auto &reader) {
        std::array<float, 4> l_old, l_new, r_old, r_new;
        this->stepConvolution(reader, prev_hrir, &l_old, &r_old);
        this->stepConvolution(reader, cur_hrir, &l_new, &r_new);
        float *out = itd_block + CHANNELS * 2 * i;
        float weight = i / (float)config::CROSSFADE_SAMPLES;
        for (unsigned int j = 0; j < 4; j++) {
          out[j] = l_new[j] * weight + l_old[j] * (1.0f - weight);
        }
        for (unsigned int j = 0; j < 4; j++) {
          out[4 + j] = r_new[j] * weight + r_old[j] * (1.0f - weight);
        }
      },
      normal_samples,
      [&](unsigned int i, auto &reader) {
        std::array<float, 4> l, r;
        this->stepConvolution(reader, cur_hrir, &l, &r);
        float *out = itd_block + CHANNELS * 2 * i;
        for (unsigned int j = 0; j < 4; j++) {
          out[j] = l[j];
        }
        for (unsigned int j = 0; j < 4; j++) {
          out[4 + j] = r[j];
        }
      });

  /*
   *pre-unrolled weights for the left and right ear.
   * Early is too little delay. Late is too much.
   */
  std::array<double, CHANNELS * 2> weights_early, weights_late;
  /* Integer delays in samples for the ears, [l r l r  l r l r...]. */
  std::array<unsigned int, CHANNELS * 2> delays;

  for (unsigned int i = 0; i < CHANNELS; i++) {
    auto [l, r] = itds[i];
    delays[i * 2] = l;
    delays[i * 2 + 1] = r;
    if (crossfade) {
      weights_late[i * 2] = l - floor(l);
      weights_early[i * 2] = 1.0 - weights_late[i * 2];
      weights_late[i * 2 + 1] = r - floor(r);
      weights_early[i * 2 + 1] = 1.0 - weights_late[i * 2 + 1];
    }
  }

  this->itd_line.runReadLoopSplit(
      config::HRTF_MAX_ITD,
      /* Crossfade the delays, if necessary. */
      crossfade_samples,
      [&](unsigned int i, auto &reader) {
        float *o = output + i * CHANNELS * 2;
        double fraction = i / (float)config::CROSSFADE_SAMPLES;
        for (unsigned int c = 0; c < CHANNELS; c++) {
          auto [old_left, old_right] = this->prev_itds[c];
          auto [new_left, new_right] = itds[c];
          double left = new_left * fraction + old_left * (1.0 - fraction);
          double right = new_right * fraction + old_right * (1.0 - fraction);
          assert(left >= 0.0);
          assert(right >= 0.0);
          unsigned int left_s = left, right_s = right;
          double wl = left - std::floor(left);
          double wr = right - std::floor(right);
          float lse = reader.read(c * 2, left_s), lsl = reader.read(c * 2, left_s + 1);
          float rse = reader.read(c * 2 + 1, right_s), rsl = reader.read(c * 2 + 1, right_s + 1);
          float ls = lsl * wl + lse * (1.0f - wl);
          float rs = rsl * wr + rse * (1.0f - wr);
          o[c * 2] += ls;
          o[c * 2 + 1] += rs;
        }
      },
      /* Then do the main loop. */
      normal_samples,
      [&](unsigned int i, auto &reader) {
        float *o = output + i * CHANNELS * 2;
        for (unsigned int j = 0; j < CHANNELS * 2; j++) {
          o[j] += reader.read(j, delays[j]);
        }
      });

  this->prev_itds = itds;
}

void HrtfPanner::setPanningAngles(unsigned int lane, double azimuth, double elevation) {
  assert(lane < CHANNELS);
  this->azimuths[lane] = azimuth;
  this->elevations[lane] = elevation;
  this->moved.set(lane, true);
}

void HrtfPanner::setPanningScalar(unsigned int lane, double scalar) {
  assert(scalar >= -1.0 && scalar <= 1.0);
  if (scalar >= 0.0) {
    this->setPanningAngles(lane, 90 * scalar, 0.0);
  } else {
    this->setPanningAngles(lane, 360.0 + 90.0 * scalar, 0.0);
  }
}

} // namespace synthizer