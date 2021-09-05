#pragma once

#include "synthizer.h"

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/block_delay_line.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/interpolated_random_sequence.hpp"
#include "synthizer/math.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/prime_helpers.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/three_band_eq.hpp"
#include "synthizer/types.hpp"

#include <pdqsort.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <numeric>
#include <utility>

namespace synthizer {

class Context;

/*
 * An FDN reverberator using a household reflection matrix and an early reflections model.
 * */
template <typename BASE> class FdnReverbEffect : public BASE {
public:
  /* Number of lines in the FDN. */
  static constexpr unsigned int LINES = 8;
  /*
   * Must be large enough to account for the maximum theoretical delay we might see. Figure that out buy summing the
   * following properties's maximums: mean_free_path + late_reflections_modulation_depth. late_reflections_delay doesn't
   * need to be accounted for, as long as its maximum is below the above sum.
   *
   * Also, add in a little bit extra so that prime numbers above the range have room, since reading the primes array can
   * go out of range.
   * */
  static constexpr unsigned int MAX_DELAY_SAMPLES = config::SR * 1;
  static constexpr float MAX_DELAY_SECONDS = 1.0f;
  /*
   * In the primes-finding algorithm, if we push up against the far end of what we can reasonably handle, we need to cap
   * it.
   *
   * The result is that extreme values will become periodic, but values this extreme aren't going to work anyway and
   * it's better than crashing.
   *
   * This must not be larger than mean free path + slack used to setmAX_DELAY_{SAMPLES,SECONDS}.
   * */
  static constexpr unsigned int MAX_FEEDBACK_DELAY = 0.35 * config::SR;

  FdnReverbEffect(const std::shared_ptr<Context> &ctx) : BASE(ctx, 1) {
    syz_BiquadConfig filter_cfg;
    syz_biquadDesignLowpass(&filter_cfg, 2000, 0.7071135624381276);
    this->setFilterInput(filter_cfg);
    /* Gain of 1.0 is almost never what people want, and everyone gets this wrong, so make FdnReverb default it lower so
     * that they have to go out of their way to set it. */
    this->setGain(0.7);
  }

  void runEffect(unsigned int block_time, unsigned int input_channels, float *input, unsigned int output_channels,
                 float *output, float gain) override;
  void resetEffect() override;
  double getEffectLingerTimeout() override;

#define PROPERTY_CLASS FdnReverbEffect
#define PROPERTY_BASE BASE
#define PROPERTY_LIST FDN_REVERB_EFFECT_PROPERTIES
#include "synthizer/property_impl.hpp"
private:
  void maybeRecomputeModel();

  /*
   * The delay, in samples, of the late reflections relative to the input.
   * */
  unsigned int late_reflections_delay_samples;

  /*
   * Used to filter the input.
   * */
  IIRFilter<1, 3, 3> input_filter;
  /*
   * The delay lines. We pack them into one to prevent needing multiple modulus.
   * */
  BlockDelayLine<LINES, nextMultipleOf(MAX_DELAY_SAMPLES, config::BLOCK_SIZE) / config::BLOCK_SIZE> lines;
  std::array<unsigned int, LINES> delays = {{0}};
  /*
   * Allows control of frequency bands on the feedback paths, so that we can have different t60s.
   * */
  ThreeBandEq<LINES> feedback_eq;
  /*
   * Random number generator for modulating the delay lines in the late reflection.
   * */
  std::array<InterpolatedRandomSequence, LINES> late_modulators;
};

template <typename BASE> void FdnReverbEffect<BASE>::maybeRecomputeModel() {
  bool dirty = false;

  /*
   * The input filter is a lowpass for now. We might let this be configurable later.
   *
   * These used to be properties, but were yanked in 0.8 in order to break compatibility all at once. For now we
   * hardcode them, but we'll be introducing a new property type specifically for filters and using that instead.
   * */
  int input_filter_enabled = 11;
  double input_filter_cutoff = 22050.0;

  /*
   * The mean free path, in seconds.
   * This is effectively meters/ speed of sound.
   * 0.01 is approximately 5 meters.
   * */
  double mean_free_path;
  dirty |= this->acquireMeanFreePath(mean_free_path);

  /*
   * The t60 of the reverb, in seconds.
   * */
  double t60;
  dirty |= this->acquireT60(t60);

  /*
   * rolloff ratios. The effective t60 for a band of the equalizer is rolloff * t60.
   * */
  double late_reflections_lf_rolloff;
  double late_reflections_hf_rolloff;
  dirty |= this->acquireLateReflectionsLfRolloff(late_reflections_lf_rolloff);
  dirty |= this->acquireLateReflectionsHfRolloff(late_reflections_hf_rolloff);

  /*
   * Defines the bands of the eq filter for late reflections.
   * */
  double late_reflections_lf_reference;
  double late_reflections_hf_reference;
  dirty |= this->acquireLateReflectionsLfReference(late_reflections_lf_reference);
  dirty |= this->acquireLateReflectionsHfReference(late_reflections_hf_reference);

  /*
   * Diffusion is a measure of how fast reflections spread. This can't be equated to a physical property, so we just
   * treat it as a percent. Internally, this feeds the algorithm which picks delay line lengths.
   * */
  double late_reflections_diffusion;
  dirty |= this->acquireLateReflectionsDiffusion(late_reflections_diffusion);

  /*
   * How much modulation in the delay lines? Larger values reduce periodicity, at the cost of introducing chorus-like
   * effects. In seconds.
   * */
  double late_reflections_modulation_depth;
  dirty |= this->acquireLateReflectionsModulationDepth(late_reflections_modulation_depth);
  /*
   * Frequency of modulation changes in hz.
   * */
  double late_reflections_modulation_frequency;
  dirty |= this->acquireLateReflectionsModulationFrequency(late_reflections_modulation_frequency);
  double late_reflections_delay;
  dirty |= this->acquireLateReflectionsDelay(late_reflections_delay);

  if (dirty == false) {
    return;
  }

  /*
   * Design the input filter. It's as straightforward as it looks.
   * */
  if (input_filter_enabled) {
    this->input_filter.setParameters(designAudioEqLowpass(input_filter_cutoff / config::SR));
  } else {
    this->input_filter.setParameters(designWire());
  }

  this->late_reflections_delay_samples = late_reflections_delay * config::SR;

  /*
   * The average delay line length is the mean free path. You can think about this as if delay lines in the FDN all ran
   * to walls. We want the average distance to those walls to be the mean free path.
   *
   * We also want the delay lines to not be tuned, which effectively means coprime. The easiest way to do that is to use
   * a set of prime numbers. Other schemes are possible, for example primes raised to powers, but some Python
   * prototyping shows that this has reasonable error, and it's easy enough to just embed an array of primes to pull
   * from.
   *
   * Finally, if we just choose all of the lines to have the mean free path as a prime number (or something close to it)
   * it'll take a long time for it to stop sounding like an echo.  We solve this by choosing some of our delay lines to
   * be much shorter, and some to be much longer. How much this spread occurs is controlled by the late reflection
   * diffusion.
   *
   * The math works like this:
   *
   * The first 2 are at the exact point of the mean free path. It would be more mathematically consistent for this just
   * to be 1, but keeping the line count at a multiple of 4 is very good for efficiency.
   *
   * The next 2 after that are at some multiples of the mean free path. At maximum diffusion, they'd be 0.5 and 1.5
   * The next 2 would be 0.25 and 1.75.
   * And so on. SO that we have:
   * (1m + 1m) + (0.5m + 1.5m) + ... = 2 + 2 + ... = 2*(n/2)*m = n*m
   * where n is the line count and m the mean free path in samples. There is obviously error here because our array is
   * an array of primes, but it's a reasonable approximation of the reality we'd like to have.
   *
   * We vary the diffusion by changing the denominator of the fraction in the above, and expose a percent-based control
   * for that purpose. Using significantly large numbers (on the order of 2) for the exponent makes strange things
   * happen for large diffusions; I tuned the constants in the below difffusion equation by ear.
   * */
  unsigned int mean_free_path_samples = mean_free_path * config::SR;
  this->delays[0] = getClosestPrime(mean_free_path_samples);
  this->delays[1] = getClosestPrimeRestricted(mean_free_path_samples, 1, &delays[0]);

  float diffusion_base = 1.0 + 0.4 * late_reflections_diffusion;
  for (unsigned int i = 2; i < LINES; i += 2) {
    unsigned int iteration = i / 2 + 1;
    float fraction = 1.0f / powf(diffusion_base, iteration);
    delays[i] = getClosestPrimeRestricted(mean_free_path_samples * fraction, i, &this->delays[0]);
    delays[i + 1] = getClosestPrimeRestricted(mean_free_path_samples * (2.0f - fraction), i + 1, &this->delays[0]);
  }

  /*
   * If the user gave us an extreme value for mean fre path, it's possible that we are going over the maximum we can
   * handle. While this  would normally be guarded by the property range mechanisms, reading from the prime array can
   * return values over the maximum.
   * */
  for (unsigned int i = 0; i < LINES; i++) {
    this->delays[i] = std::min(this->delays[i], MAX_FEEDBACK_DELAY);
  }

  /*
   * In order to minimize clipping and/or other crossfading artifacts, keep the lines sorted from least
   * delay to greatest.
   * */
  pdqsort_branchless(this->delays.begin(), this->delays.end());

  /*
   * The gain for each line is computed from t60, the time to decay to -60 DB.
   *
   * The math to prove why this works is involved, see:
   * https://ccrma.stanford.edu/~jos/pasp/Achieving_Desired_Reverberation_Times.html What we do works for any matrix,
   * but an intuitive (but inaccurate) explanation of our specific case follows:
   *
   * Imagine that samples traveling through the FDN are balls on a track, choosing some random path. What matters for
   * the decay curve is how fast the ball slows down, i.e. the friction in this analogy.  If the ball has traveled 1
   * second and our t60 is 1 second, we want it to have "slowed down" by 60db. This implies that the track has uniform
   * friction.  Each line has delay samples of track, so we can work out the gain by determining what it should be per
   * sample, and multiplying by the length of the track.
   *
   * We control this with an equalizer, which lets the user control how "bright" the reverb is.
   *
   * be careful to guard against t60 = 0.  We could have done this by constraining the range of the propperty, but
   * it is convenient for external consumers to be able to use 0 and t60 is already inaccurate, so a little bit more
   * doesn't hurt anything.
   * */
  float decay_per_sample_db = -60.0f / (t60 + 0.001) / config::SR;
  for (unsigned int i = 0; i < LINES; i++) {
    unsigned int length = this->delays[i];
    float decay_db = length * decay_per_sample_db;
    ThreeBandEqParams params;
    params.dbgain_lower = decay_db * late_reflections_lf_rolloff;
    params.freq_lower = late_reflections_lf_reference;
    params.dbgain_mid = decay_db;
    params.dbgain_upper = decay_db * late_reflections_hf_rolloff;
    params.freq_upper = late_reflections_hf_reference;
    this->feedback_eq.setParametersForLane(i, params);
  }

  /*
   * The modulation depth and rate.
   * */
  unsigned int mod_depth_in_samples = late_reflections_modulation_depth * config::SR;
  unsigned int mod_rate_in_samples = config::SR / late_reflections_modulation_frequency;
  for (unsigned int i = 0; i < LINES; i++) {
    /*
     * vary the frequency slightly so that they're not all switching exactly on the same sample.
     * This smoothes out the peak of what is effectively a triangle wave.
     * */
    this->late_modulators[i] = InterpolatedRandomSequence(this->late_modulators[i].tick(), mod_rate_in_samples + i,
                                                          0.0f, mod_depth_in_samples);
  }
}

template <typename BASE> void FdnReverbEffect<BASE>::resetEffect() {
  this->lines.clear();
  this->feedback_eq.reset();
  this->input_filter.reset();
}

template <typename BASE>
void FdnReverbEffect<BASE>::runEffect(unsigned int block_time, unsigned int input_channels, float *input,
                                      unsigned int output_channels, float *output, float gain) {
  (void)block_time;

  /*
   * Output is stereo. For surround setups, we can get 99% of the benefit just by upmixing stereo differently, which
   * we'll do in future by hand as a special case.
   * */
  auto output_buf_guard = acquireBlockBuffer();
  float *const output_buf_ptr = output_buf_guard;

  this->maybeRecomputeModel();

  /*
   * Compute the max delay. This needs to be done here because
   * modulation has to factor in.
   * */
  unsigned int max_delay = 0;
  for (unsigned int i = 0; i < LINES; i++) {
    /*
     * account for the extra sample we need when interpolating here.
     * Also account for any floating point issues by adding one more sample. Shouldn't be necessary, but won't hurt
     * anything and may protect against bugs if interpolatedRandomSequence is ever modified.
     * */
    unsigned int new_max_delay = this->delays[i] + this->late_modulators[i].getMaxValue() + 2;
    max_delay = std::max(new_max_delay, max_delay);
  }
  max_delay = std::max(max_delay, this->late_reflections_delay_samples);

  /*
   * Normally we would go through a temporary buffer to premix channels here, and we still might, but we're using the
   * delay line's write loop and it's always to mono; just do it inline.
   * */
  this->lines.runRwLoop(max_delay, [&](unsigned int i, auto &rw) {
    float *input_ptr = input + input_channels * i;
    float input_sample = 0.0f;

    /* Mono downmix. */
    for (unsigned int ch = 0; ch < input_channels; ch++) {
      input_sample += input_ptr[ch];
    }
    input_sample *= 1.0f / input_channels;

    /*
     * Apply the initial filter. The purpose is to let the user make the reverb less harsh on high-frequency sounds.
     * */
    this->input_filter.tick(&input_sample, &input_sample);

    /*
     * Implement a householder reflection about <1, 1, 1, 1... >. This is a reflection about the hyperplane.
     * */

    /* not initialized because zeroing can be expensive and we set it immediately in the loop below. */
    std::array<float, LINES> values;
    for (unsigned int j = 0; j < LINES; j++) {
      double delay = this->delays[j] + this->late_modulators[j].tick();
      float w2 = delay - std::floor(delay);
      float w1 = 1.0f - w2;
      float v1 = rw.read(j, delay);
      float v2 = rw.read(j, delay + 1);
      values[j] = v1 * w1 + v2 * w2;
    }

    /*
     * Pass it through the equalizer, which handles feedback.
     * */
    this->feedback_eq.tick(&values[0], &values[0]);

    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    sum *= 2.0f / LINES;

    /*
     * Write it back.
     * */
    float input_per_line = input_sample * (1.0f / LINES);
    for (unsigned int j = 0; j < LINES; j++) {
      rw.write(j, values[j] - sum + input_per_line);
    }

    /*
     * We want two decorrelated channels, with roughly the same amount of reflections in each.
     * */
    unsigned int d = this->late_reflections_delay_samples;
    float *frame = rw.readFrame(d);
    float l = frame[0] + frame[2] + frame[4] + frame[6];
    float r = frame[1] + frame[3] + frame[5] + frame[7];
    output_buf_ptr[2 * i] = l * gain;
    output_buf_ptr[2 * i + 1] = r * gain;
  });

  mixChannels(config::BLOCK_SIZE, output_buf_ptr, 2, output, output_channels);
}

template <typename BASE> double FdnReverbEffect<BASE>::getEffectLingerTimeout() { return this->getT60(); }

} // namespace synthizer
