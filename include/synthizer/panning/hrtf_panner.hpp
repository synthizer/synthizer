#pragma once

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/data/hrtf.hpp"
#include "synthizer/math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vbool.hpp"

#include <boost/predef.h>

#ifdef BOOST_HW_SIMD_X86
#include <emmintrin.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <tuple>

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
void computeHrtfImpulses(double azimuth, double elevation, float *left, float *right);

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
  /*
   * Must be at least 1 block, and at least hrtf::IMPULSE_LENGTH. But we want longer; longer requires less modulus.
   * */
  BlockDelayLine<1, nextPowerOfTwo(config::HRTF_MAX_ITD + config::BLOCK_SIZE * 10) / config::BLOCK_SIZE> input_line;

  /* This is an extra sample long to make linear interpolation easier. */
  BlockDelayLine<1, nextPowerOfTwo(config::BLOCK_SIZE * 10 + config::HRTF_MAX_ITD + 1) / config::BLOCK_SIZE>
      itd_line_left, itd_line_right;

  /*
   * The hrirs and an index which determines which we are using.
   *
   * The way this works is that current_hrir is easily flipped with ^, so each array holds two arrays of the hrir for
   * that ear.
   *
   * These impulses are stored in reversed order, which lets us go "forward" both in the delay line and the impulse
   * array.  This greatly helps vectorization and SIMD.
   * */
  std::array<std::array<float, data::hrtf::IMPULSE_LENGTH>, 2> impulse_l, impulse_r;

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
                                            const data::hrtf::ElevationDef *elev_upper, float *out) {
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
    out[i] = impulses[0][i] * weights[0];
  }

  for (unsigned int c = 1; c < weight_count; c++) {

    for (unsigned int i = 0; i < data::hrtf::IMPULSE_LENGTH; i++) {
      out[i] += impulses[c][i] * weights[c];
    }
  }
}

inline void computeHrtfImpulses(double azimuth, double elevation, float *left, float *right) {
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

  computeHrtfImpulseSingleChannel(azimuth, elevation, elev_lower, elev_upper, left);
  computeHrtfImpulseSingleChannel(360 - azimuth, elevation, elev_lower, elev_upper, right);
}

inline unsigned int HrtfPanner::getOutputChannelCount() { return 2; }

inline float *HrtfPanner::getInputBuffer() { return this->input_line.getNextBlock(); }

namespace hrtf_panner_detail {
template <typename MP>
inline void stepConvolution(MP ptr, const float *hrir_left, const float *hrir_right, float *dest_l, float *dest_r) {
  static_assert(data::hrtf::IMPULSE_LENGTH > 8);
  static_assert(data::hrtf::IMPULSE_LENGTH % 8 == 0);

  // The following horrible code was arrived at through benchmarking because MSVC flat out refuses to recognize any of
  // this as vectorizable.  We'll continue this effort in the future with overloads using the fact that ModPointer is
  // sometimes a pointer, but for now we take what we can get.

  float redl0 = 0.0f, redl1 = 0.0f, redl2 = 0.0f, redl3 = 0.0f;
  float redr0 = 0.0f, redr1 = 0.0f, redr2 = 0.0f, redr3 = 0.0f;

  // We store the impulses reversed so we can go "forward".  To do so, get the pointer at the beginning and then bump it
  // up.
  ptr = ptr - data::hrtf::IMPULSE_LENGTH + 1;

  for (unsigned int j = 0; j < data::hrtf::IMPULSE_LENGTH; j += 8) {
#define DECL(X) float s##X = ptr[j + X], l##X = hrir_left[j + X], r##X = hrir_right[j + X]
    DECL(0);
    DECL(1);
    DECL(2);
    DECL(3);
    DECL(4);
    DECL(5);
    DECL(6);
    DECL(7);

#undef DECL

#define RES(X) float rl##X = s##X * l##X, rr##X = s##X * r##X

    RES(0);
    RES(1);
    RES(2);
    RES(3);
    RES(4);
    RES(5);
    RES(6);
    RES(7);

#undef RES

    redl0 += rl0 + rl1;
    redl1 += rl2 + rl3;
    redl2 += rl4 + rl5;
    redl3 += rl6 + rl7;
    redr0 += rr0 + rr1;
    redr1 += rr2 + rr3;
    redr2 += rr4 + rr5;
    redr3 += rr6 + rr7;
  }

  *dest_l = (redl0 + redl1) + (redl2 + redl3);
  *dest_r = (redr0 + redr1) + (redr2 + redr3);
}

// Since MSVC refuses to vectorize the proceeding generic version of this well, and since Clang could vectorize better
// versions but that hurts MSVC, we carve out the following optimized implementation using intrinsics.
#ifdef BOOST_HW_SIMD_X86
inline float horizontalSum(__m128 input) {
  __m128 halves_swapped = _mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 0, 3, 2));
  __m128 halves_summed = _mm_add_ps(input, halves_swapped);
  __m128 subhalves = _mm_shuffle_ps(halves_swapped, halves_swapped, _MM_SHUFFLE(2, 3, 0, 1));
  return _mm_cvtss_f32(_mm_add_ps(subhalves, halves_summed));
}

inline void stepConvolution(float *ptr, const float *hrir_left, const float *hrir_right, float *dest_l, float *dest_r) {
  static_assert(data::hrtf::IMPULSE_LENGTH > 4);
  static_assert(data::hrtf::IMPULSE_LENGTH % 4 == 0);

  __m128 acc_l = _mm_setzero_ps();
  __m128 acc_r = _mm_setzero_ps();

  ptr = ptr - data::hrtf::IMPULSE_LENGTH + 1;
  for (unsigned int i = 0; i < data::hrtf::IMPULSE_LENGTH; i += 4) {
    __m128 sample = _mm_loadu_ps(ptr + i);
    __m128 impulse_left = _mm_loadu_ps(hrir_left + i);
    __m128 impulse_right = _mm_loadu_ps(hrir_right + i);
    __m128 tmp_l = _mm_mul_ps(sample, impulse_left);
    __m128 tmp_r = _mm_mul_ps(sample, impulse_right);
    acc_l = _mm_add_ps(tmp_l, acc_l);
    acc_r = _mm_add_ps(tmp_r, acc_r);
  }

  *dest_l = horizontalSum(acc_l);
  *dest_r = horizontalSum(acc_r);
}
#endif
} // namespace hrtf_panner_detail

inline void HrtfPanner::run(float *output) {
  float *prev_hrir_l = nullptr;
  float *prev_hrir_r = nullptr;
  float *cur_hrir_l = this->impulse_l[this->current_hrir].data();
  float *cur_hrir_r = this->impulse_r[this->current_hrir].data();

  bool crossfade = this->moved;
  this->moved = false;

  if (crossfade) {
    prev_hrir_l = cur_hrir_l;
    prev_hrir_r = cur_hrir_r;
    this->current_hrir ^= 1;
    cur_hrir_l = this->impulse_l[this->current_hrir].data();
    cur_hrir_r = this->impulse_r[this->current_hrir].data();
  }

  if (crossfade) {
    computeHrtfImpulses(this->azimuth, this->elevation, cur_hrir_l, cur_hrir_r);
    std::reverse(cur_hrir_l, cur_hrir_l + data::hrtf::IMPULSE_LENGTH);
    std::reverse(cur_hrir_r, cur_hrir_r + data::hrtf::IMPULSE_LENGTH);
  }

  unsigned int crossfade_samples = crossfade ? config::CROSSFADE_SAMPLES : 0;

  float *itd_block_left = this->itd_line_left.getNextBlock();
  float *itd_block_right = this->itd_line_right.getNextBlock();

  auto input_mp = this->input_line.getModPointer(data::hrtf::IMPULSE_LENGTH - 1);
  std::visit(
      [&](auto &ptr) {
        for (std::size_t i = 0; i < crossfade_samples; i++) {
          float l_old, l_new, r_old, r_new;
          hrtf_panner_detail::stepConvolution(ptr, prev_hrir_l, prev_hrir_r, &l_old, &r_old);
          hrtf_panner_detail::stepConvolution(ptr, cur_hrir_l, cur_hrir_r, &l_new, &r_new);
          float w1 = i / (float)config::CROSSFADE_SAMPLES;
          float w0 = 1.0f - w1;

          itd_block_left[i] = l_old * w0 + l_new * w1;
          itd_block_right[i] = r_old * w0 + r_new * w1;

          ++ptr;
        }

        for (std::size_t i = crossfade_samples; i < config::BLOCK_SIZE; i++) {
          hrtf_panner_detail::stepConvolution(ptr, cur_hrir_l, cur_hrir_r, &itd_block_left[i], &itd_block_right[i]);
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

  // Let's figure out if we can use less than the max hrtf delay.

  unsigned int needed_itd_l = config::HRTF_MAX_ITD, needed_itd_r = config::HRTF_MAX_ITD;
  bool left_is_zero = itd_l == 0.0f && this->prev_itd_l == 0.0f;
  bool right_is_zero = itd_r == 0.0f && this->prev_itd_r == 0.0f;

  // The max delay for either side is either 0, by the above check, or 1 more than the maximum of the current and
  // previous ITD.  Since the steady state has the current and previous ITD as the same, the max simplifies in that case
  // to just the current ITD.
  needed_itd_l = left_is_zero ? 0 : std::max(itd_l_i, (unsigned int)this->prev_itd_l) + 1;
  needed_itd_r = right_is_zero ? 0 : std::max(itd_r_i, (unsigned int)this->prev_itd_r) + 1;

  auto itd_left_mp = this->itd_line_left.getModPointer(needed_itd_l);
  auto itd_right_mp = this->itd_line_right.getModPointer(needed_itd_r);

  std::visit(
      [&](auto ptr_left, auto ptr_left_zero, auto ptr_right, auto ptr_right_zero) {
        unsigned int i = 0;
        float prev_itd_l = this->prev_itd_l, prev_itd_r = this->prev_itd_r;

        for (; i < crossfade_samples; i++) {
          float *o = output + i * 2;
          float itd_w1 = i * (1.0f / (float)config::CROSSFADE_SAMPLES);
          float itd_w2 = 1.0f - itd_w1;

          float left = itd_l * itd_w1 + prev_itd_l * itd_w2;
          float right = itd_r * itd_w1 + prev_itd_r * itd_w2;
          assert(left >= 0.0);
          assert(right >= 0.0);
          unsigned int left_s = left, right_s = right;
          float wl = left - left_s;
          float wr = right - right_s;
          auto del_left_ptr = ptr_left - (left_s + 1);
          auto del_right_ptr = ptr_right - (right_s + 1);
          float lse = del_left_ptr[1], lsl = del_left_ptr[0];
          float rse = del_right_ptr[1], rsl = del_right_ptr[0];
          float ls = lsl * wl + lse * (1.0f - wl);
          float rs = rsl * wr + rse * (1.0f - wr);
          o[0] += ls;
          o[1] += rs;
          ++ptr_left;
          ++ptr_right;
        }

        for (; i < config::BLOCK_SIZE; i++) {
          float *o = output + i * 2;

          if (ptr_left_zero) {
            o[0] = *ptr_left;
          } else {
            auto del_left_ptr = ptr_left - (itd_l_i + 1);
            float lse = del_left_ptr[1], lsl = del_left_ptr[0];
            o[0] += itd_w_early_l * lse + itd_w_late_l * lsl;
          }

          if (ptr_right_zero) {
            o[1] = *ptr_right;
          } else {
            auto del_right_ptr = ptr_right - (itd_r_i + 1);
            float rse = del_right_ptr[1], rsl = del_right_ptr[0];
            o[1] += itd_w_early_r * rse + itd_w_late_r * rsl;
          }

          ++ptr_left;
          ++ptr_right;
        }
      },
      itd_left_mp, vCond(left_is_zero), itd_right_mp, vCond(right_is_zero));

  this->itd_line_left.incrementBlock();
  this->itd_line_right.incrementBlock();

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
