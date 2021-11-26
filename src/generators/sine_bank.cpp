#include "synthizer/generators/sine_bank.hpp"

#include "synthizer.h"

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/math.hpp"

#include <math.h>
#include <memory>
#include <optional>
#include <vector>

namespace synthizer {

SineBankGenerator::SineBankGenerator(const std::shared_ptr<Context> &context, const syz_SineBankConfig *cfg)
    : Generator(context), bank(cfg->initial_frequency) {
  for (unsigned int i = 0; i < cfg->wave_count; i++) {
    SineWaveConfig wave{cfg->waves[i].frequency_mul, cfg->waves[i].phase, cfg->waves[i].gain};
    this->bank.addWave(wave);
  }

  // Make sure the property subsystem starts at the right place.
  this->setFrequency(cfg->initial_frequency, false);
}

int SineBankGenerator::getObjectType() { return SYZ_OTYPE_SINE_BANK_GENERATOR; }

unsigned int SineBankGenerator::getChannels() { return 1; }

void SineBankGenerator::generateBlock(float *output, FadeDriver *gain_driver) {
  // For now, we round-trip through a temporary buffer unconditionally to apply the gain; in future, this restriction
  // may be lifted due to improvements in the underlying fade architecture.  Note that as is standard with Synthizer
  // objects, the bank adds.
  auto tmp_buffer = acquireBlockBuffer(true);

  this->bank.setFrequency(this->getFrequency());
  this->bank.fillBlock<config::BLOCK_SIZE>(tmp_buffer);

  gain_driver->drive(this->getContextRaw()->getBlockTime(), [&](auto &&gain_cb) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      output[i] += tmp_buffer[i] * gain_cb(i);
    }
  });
}

std::optional<double> SineBankGenerator::startGeneratorLingering() {
  this->setGain(0.0);
  return 2 * (config::BLOCK_SIZE / (double)config::SR);
}

/**
 * Apply the Lanczos sigma approximation to some sine waves, assuming that they are in order and that the first wave is
 * the fundamental.  This function assumes that these are integral harmonics and that the vec is non-empty and in order
 * of increasing frequency.
 *
 * This is a strategy to reduce the gibbs phenomenon when approximating fourier series.
 * */
static void sigmaApproximate(std::vector<syz_SineBankWave> *waves) {
  // Per wolfram, rewrite the fourier series as:
  // `f(theta)=1/2a_0+sum_(n=1)^(m-1)sinc((npi)/(2m))[a_ncos(ntheta)+b_nsin(ntheta)]` where `m`  is the last term.
  //
  // Wolfram is unclear on the meaning of "last term" here: in the normal fourier series, the last term would not be
  // included  by the above sum.  There is another definition which they claim is incorrect where the upper end of the
  // sum is `m`, plus a few other variations.  It probably doesn't matter much as long as we never evaluate the sinc
  // function outside the interval `(0, pi/2]` (0 is a special case, and not included by any definition, and any value
  // after `pi/2` is negative).  As a consequence we take the interpretation that `m= h + 1` where h is the highest
  // harmonic, and split the difference.  At the end of the day, if this sounds good enough that's all we care about.

  double m = waves->back().frequency_mul + 1;
  // should always be exactly an integer.
  assert((unsigned int)m == m);

  // In practice, the above is actually a multiplication on the gain of each term, and we already have those gains. To
  // do this, run over the array and multiply them in.
  for (auto &w : *waves) {
    double n = w.frequency_mul;
    // Sinc function.  We know the parameter can never be zero.
    double sigma = sin(PI * n / (2 * m));
    w.gain *= sigma;
  }
}

/**
 * Normalize a set of waves.
 * */
static void normalizeSeries(std::vector<syz_SineBankWave> *waves) {
  double gsum = 0.0;
  for (auto &w : *waves) {
    gsum += w.gain;
  }
  double nf = 1.0 / gsum;
  for (auto &w : *waves) {
    w.gain *= nf;
  }
}

static std::vector<syz_SineBankWave> buildSquareSeries(unsigned int partials) {
  std::vector<syz_SineBankWave> out{};
  for (unsigned int p = 0; p < partials; p++) {
    double gain = 1.0 / (2 * p + 1);
    out.emplace_back(syz_SineBankWave{(double)(p * 2 + 1), 0.0, gain});
  }

  sigmaApproximate(&out);
  normalizeSeries(&out);
  return out;
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI void syz_initSineBankConfig(struct syz_SineBankConfig *cfg) { *cfg = syz_SineBankConfig{}; }

SYZ_CAPI syz_ErrorCode syz_createSineBankGenerator(syz_Handle *out, syz_Handle context,
                                                   struct syz_SineBankConfig *bank_config, void *config, void *userdata,
                                                   syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;
  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<SineBankGenerator>(bank_config);
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createSineBankGeneratorSineWave(syz_Handle *out, syz_Handle context,
                                                           double initial_frequency, void *config, void *userdata,
                                                           syz_UserdataFreeCallback *userdata_free_callback) {
  static const struct syz_SineBankWave wave { 1.0, 0.0, 1.0 };
  struct syz_SineBankConfig cfg;

  cfg.waves = &wave;
  cfg.wave_count = 1;
  cfg.initial_frequency = initial_frequency;
  return syz_createSineBankGenerator(out, context, &cfg, config, userdata, userdata_free_callback);
}

static syz_ErrorCode createSineBankFromVec(syz_Handle *out, syz_Handle context, double initial_frequency,
                                           std::vector<syz_SineBankWave> *waves, void *userdata,
                                           syz_UserdataFreeCallback *userdata_free_callback) {
  struct syz_SineBankConfig cfg;

  cfg.waves = &(*waves)[0];
  cfg.wave_count = waves->size();
  cfg.initial_frequency = initial_frequency;
  return syz_createSineBankGenerator(out, context, &cfg, NULL, userdata, userdata_free_callback);
}

SYZ_CAPI syz_ErrorCode syz_createSineBankGeneratorSquareWave(syz_Handle *out, syz_Handle context,
                                                             double initial_frequency, unsigned int partials,
                                                             void *config, void *userdata,
                                                             syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;
  auto waves = buildSquareSeries(partials);
  return createSineBankFromVec(out, context, initial_frequency, &waves, userdata, userdata_free_callback);

  SYZ_EPILOGUE
}
