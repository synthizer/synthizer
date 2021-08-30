#pragma once

#include <algorithm>
#include <cstddef>
#include <tuple>

#include "synthizer/bitset.hpp"
#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/data/hrtf.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * HRTF implementation.
 *
 * This involves a variety of components which interact with one another to provide efficient HRTF processing of audio
 * data:
 *
 * - Low-level functions, which compute HRIRs and interaural time delay.
 * - A higher level piece, the HrtfPannerBlock, which does HRTF on 4 sources at once.
 * - A yet higher level piece, the HrtfPannerBank, which implements the panner bank interface.
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
 * A bank of 4 parallel HRTF convolvers with ITD.
 *
 * We don't template this because anything bigger than 4 is likely as not to cause problems, and it's convenient to not
 * have all of this code inline. That said, note the class-level consts: this is intentionally written so that it can be
 * templatized later.
 * */
class HrtfPanner : public AbstractPanner {
public:
  static constexpr std::size_t CHANNELS = 4;

  HrtfPanner();
  unsigned int getOutputChannelCount();
  unsigned int getLaneCount();
  std::tuple<float *, unsigned int> getLane(unsigned int channel);
  void releaseLane(unsigned int channel);
  void run(float *output);
  void setPanningAngles(unsigned int lane, double azimuth, double elevation);
  void setPanningScalar(unsigned int lane, double scalar);

private:
  template <typename R>
  void stepConvolution(R &&reader, const float *hrir, std::array<float, 4> *dest_l, std::array<float, 4> *dest_r);

  /*
   * 2 blocks plus the max ITD.
   * We'll use modulus on 1/3 blocks on average.
   * */
  BlockDelayLine<CHANNELS,
                 nextMultipleOf(config::HRTF_MAX_ITD + config::BLOCK_SIZE * 2, config::BLOCK_SIZE) / config::BLOCK_SIZE>
      input_line;
  /* This is an extra sample long to make linear interpolation easier. */
  BlockDelayLine<CHANNELS * 2, nextMultipleOf(config::BLOCK_SIZE * 2 + config::HRTF_MAX_ITD + 1, config::BLOCK_SIZE) /
                                   config::BLOCK_SIZE>
      itd_line;

  /*
   * The hrirs and an index which determines which we are using.
   *
   * The way this works is that current_hrir is easily flipped with xor. current_hrir ^ 1 is the previous, (current_hrir
   * ^ 1)*CHANNELS*2*data::hrtf::RESPONSE_LENGTH is the index of the previous.
   *
   * These are stored like: [l1, l2, l3, l4, r1, r2, r3, r4]
   * We do the convolution with a specialized kernel that can deal with this.
   * */
  std::array<float, data::hrtf::IMPULSE_LENGTH *CHANNELS * 2 * 2> hrirs = {0.0f};
  unsigned int current_hrir = 0;
  std::array<std::tuple<double, double>, CHANNELS> prev_itds{};
  std::array<double, CHANNELS> azimuths = {{0.0}};
  std::array<double, CHANNELS> elevations = {{0.0}};
  Bitset<CHANNELS> moved;
};

} // namespace synthizer
