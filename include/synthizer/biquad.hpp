#pragma once

#include "synthizer.h"

#include <memory>

/*
 * Type-erased biquad filters intended for external consumers.
 *
 * These are primarily designed to back properties and for the moment can only process one block of audio
 * at a time.  This is because the crossfade is difficult to implement
 * when ticking sample-by-sample and trying to use the block-based interface. It would be possible to extend this to
 * support that, but internal components that aren't getting filter configuration from consumers can simply use the
 * IIRFilter.
 *
 * When reconfigured, these will crossfade between the two filters, giving the new one time to reach a steady state.
 *
 * Internally, these are implemented as specialized templates, one per channel count, up to config::MAX_CHANNELS.
 * */
namespace synthizer {

class BiquadFilter {
public:
  virtual ~BiquadFilter() {}
  /*
   * Adds to the output, as usual.
   * If in and out overlap, set add = false.
   * */
  virtual void processBlock(float *in, float *out, bool add = true) = 0;
  virtual void configure(const syz_BiquadConfig &config) = 0;
};

/*
 * Get a specialized BiquadFilter implementation.
 * Will succeed for up to MAX_CHANNELS; values above that crash with an assert as going higher is programmer error.
 * The returned filter is configured for identity.
 * */
std::shared_ptr<BiquadFilter> createBiquadFilter(unsigned int channels);

} // namespace synthizer
