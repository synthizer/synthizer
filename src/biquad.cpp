#include "synthizer/biquad.hpp"

#include "synthizer/config.hpp"
#include "synthizer/filter_design.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/memory.hpp"

#include <array>
#include <memory>

namespace synthizer {

template <unsigned int CHANNELS> class ConcreteBiquadFilter : public BiquadFilter {
public:
  ConcreteBiquadFilter();

  void processBlock(float *in, float *out, bool add = true) override;
  void configure(const syz_BiquadConfig &config) override;

private:
  template <bool ADD, bool CROSSFADING> void processBlockImpl(float *in, float *out);
  std::array<IIRFilter<CHANNELS, 3, 3>, 2> filters;
  BiquadFilterDef filter_def;
  bool crossfade = false;
  /* If there's a configuration before the first block, apply it to both filters. Prevents crossfading the first block
   * from a wire. */
  bool first_block = true;
  /* The filter is currently configured as a wire, e.g.  copy and/or add. */
  bool is_wire = true;
  /* Points to the index of the active filter in the array. */
  unsigned char active = 0;
};

template <unsigned int CHANNELS> ConcreteBiquadFilter<CHANNELS>::ConcreteBiquadFilter() {
  this->filter_def = designWire();
  this->filters[0].setParameters(designWire());
  this->filters[1].setParameters(designWire());
}

template <unsigned int CHANNELS> void ConcreteBiquadFilter<CHANNELS>::configure(const syz_BiquadConfig &config) {
  BiquadFilterDef def;
  def.num_coefs[0] = config._b0;
  def.num_coefs[1] = config._b1;
  def.num_coefs[2] = config._b2;
  def.den_coefs[0] = config._a1;
  def.den_coefs[1] = config._a2;
  def.gain = config._gain;

  if (def == this->filter_def) {
    /* Filter is the same. Crossfading would produce artifacts for no reason since inactive has to reach steady state,
     * so stop now. */
    return;
  }
  this->is_wire = config._is_wire != 0;
  this->filter_def = def;
  this->filters[this->active ^ 1].setParameters(this->filter_def);
  if (this->first_block) {
    this->filters[this->active].setParameters(this->filter_def);
  }
  this->crossfade = true;
}

template <unsigned int CHANNELS> void ConcreteBiquadFilter<CHANNELS>::processBlock(float *in, float *out, bool add) {
  this->first_block = false;

  if (add) {
    if (this->crossfade) {
      this->processBlockImpl<true, true>(in, out);
    } else {
      this->processBlockImpl<true, false>(in, out);
    }
  } else {
    if (this->crossfade) {
      this->processBlockImpl<false, true>(in, out);
    } else {
      this->processBlockImpl<false, false>(in, out);
    }
  }

  if (this->crossfade) {
    /*
     * We only ever crossfade for 1 block, after reconfiguring. Stop crossfading and flip
     * the active filter.
     * */
    this->crossfade = false;
    this->filters[this->active].reset();
    this->filters[this->active].setParameters(this->filter_def);
    this->active ^= 1;
  }
}

template <unsigned int CHANNELS>
template <bool ADD, bool CROSSFADE>
void ConcreteBiquadFilter<CHANNELS>::processBlockImpl(float *in, float *out) {
  const float gain_inv = 1.0f / config::BLOCK_SIZE;
  IIRFilter<CHANNELS, 3, 3> *active_filt = &this->filters[this->active];
  IIRFilter<CHANNELS, 3, 3> *inactive_filt = &this->filters[this->active ^ 1];

  /*
   * The wire case is either a copy or add depending on ADD.
   * But we can only optimize it if we aren't crossfading, or there will be artifacts.
   * */
  if (CROSSFADE == false && this->is_wire) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE * CHANNELS; i++) {
      out[i] = ADD ? out[i] + in[i] : in[i];
    }
    return;
  }

  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    float tmp[CHANNELS] = {0.0f};
    float *in_frame = in + CHANNELS * i;
    /*
     * Unfortunately the IIRFilter always sets, because it's old enough
     * to be from before Synthizer established firm conventions.
     *
     * It's also important that if in == out and we're crossfading, we don't directly write to the output buffer before
     * the crossfade is computed.
     * */
    float *out_frame = ADD || CROSSFADE ? &tmp[0] : out + CHANNELS * i;

    active_filt->tick(in_frame, out_frame);
    if (CROSSFADE) {
      /*
       * Fade inactive out, fade active in.
       * */
      float cf_tmp[CHANNELS] = {0.0f};
      inactive_filt->tick(in_frame, cf_tmp);
      /* We are fading active out in favor of inactive. processBlock flips them at the end. */
      float inactive_w = i * gain_inv;
      float active_w = 1.0f - inactive_w;
      for (unsigned int c = 0; c < CHANNELS; c++) {
        tmp[c] = cf_tmp[c] * inactive_w + tmp[c] * active_w;
      }
    }

    if (CROSSFADE || ADD) {
      float *real_out = out + CHANNELS * i;

      /* We used the intermediate buffer, so copy out. */
      for (unsigned int c = 0; c < CHANNELS; c++) {
        float res = ADD ? out_frame[c] + tmp[c] : tmp[c];
        real_out[c] = res;
      }
    }
    /* If we didn't crossfade or add, it's already where it should be. */
  }
}

template <unsigned int CHANNELS> static std::shared_ptr<BiquadFilter> biquadFilterFactory() {
  auto obj = new ConcreteBiquadFilter<CHANNELS>();
  auto ret = sharedPtrDeferred<ConcreteBiquadFilter<CHANNELS>>(
      obj, [](ConcreteBiquadFilter<CHANNELS> *o) { deferredDelete(o); });
  return std::static_pointer_cast<BiquadFilter>(ret);
}

/*
 * rather than use std::array here, we use boring C-like arrays. This is because
 * we want to infer then static_assert the length, which we can't do if we use std::array (which won't defer).
 * */
typedef std::shared_ptr<BiquadFilter> BiquadFilterFactoryCb();
static BiquadFilterFactoryCb *factories[] = {
    biquadFilterFactory<1>,  biquadFilterFactory<2>,  biquadFilterFactory<3>,  biquadFilterFactory<4>,
    biquadFilterFactory<5>,  biquadFilterFactory<6>,  biquadFilterFactory<7>,  biquadFilterFactory<8>,
    biquadFilterFactory<9>,  biquadFilterFactory<10>, biquadFilterFactory<11>, biquadFilterFactory<12>,
    biquadFilterFactory<13>, biquadFilterFactory<14>, biquadFilterFactory<15>, biquadFilterFactory<16>,
};

static_assert(sizeof(factories) / sizeof(factories[0]) == config::MAX_CHANNELS,
              "Neeed to add/remove biquad factories if MAX_CHANNELS is changed");

std::shared_ptr<BiquadFilter> createBiquadFilter(unsigned int channels) {
  assert(channels > 0);
  assert(channels <= config::MAX_CHANNELS);
  return factories[channels - 1]();
}

} // namespace synthizer
