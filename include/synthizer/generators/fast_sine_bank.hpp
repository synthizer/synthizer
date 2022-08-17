#pragma once

#include "synthizer.h"

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/context.hpp"
#include "synthizer/fast_sine_bank.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/property_internals.hpp"

#include <memory>
#include <optional>

namespace synthizer {

class Context;

class FastSineBankGenerator : public Generator {
public:
  FastSineBankGenerator(const std::shared_ptr<Context> &context, const syz_SineBankConfig *cfg);

  int getObjectType() override;
  unsigned int getChannels() override;
  void generateBlock(float *output, FadeDriver *gain_driver) override;
  std::optional<double> startGeneratorLingering() override;

#define PROPERTY_CLASS SineBankGenerator
#define PROPERTY_BASE Generator
#define PROPERTY_LIST SINE_BANK_GENERATOR_PROPERTIES
#include "synthizer/property_impl.hpp"

private:
  FastSineBank bank;
};

inline FastSineBankGenerator::FastSineBankGenerator(const std::shared_ptr<Context> &context,
                                                    const syz_SineBankConfig *cfg)
    : Generator(context), bank(cfg->initial_frequency) {
  for (unsigned int i = 0; i < cfg->wave_count; i++) {
    SineWaveConfig wave{cfg->waves[i].frequency_mul, cfg->waves[i].phase, cfg->waves[i].gain};
    this->bank.addWave(wave);
  }

  // Make sure the property subsystem starts at the right place.
  this->setFrequency(cfg->initial_frequency, false);
}

inline int FastSineBankGenerator::getObjectType() { return SYZ_OTYPE_FAST_SINE_BANK_GENERATOR; }

inline unsigned int FastSineBankGenerator::getChannels() { return 1; }

// The weird parameter name gd is due to what is basically a false positive warning from MSVC, which doesn't like that
// the base class has a private member called gain_driver.
inline void FastSineBankGenerator::generateBlock(float *output, FadeDriver *gd) {
  // For now, we round-trip through a temporary buffer unconditionally to apply the gain; in future, this restriction
  // may be lifted due to improvements in the underlying fade architecture.  Note that as is standard with Synthizer
  // objects, the bank adds.
  auto tmp_buffer = acquireBlockBuffer(true);

  this->bank.setFrequency(this->getFrequency());
  this->bank.fillBlock<config::BLOCK_SIZE>(tmp_buffer);

  gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &&gain_cb) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      output[i] += tmp_buffer[i] * gain_cb(i);
    }
  });
}

inline std::optional<double> FastSineBankGenerator::startGeneratorLingering() {
  this->setGain(0.0);
  return 2 * (config::BLOCK_SIZE / (double)config::SR);
}

/* Helpers for the sine bank constructors. */
namespace sb_construction_helpers {
/**
 * Apply the Lanczos sigma approximation to some sine waves, assuming that they are in order and that the first wave is
 * the fundamental.  This function assumes that these are integral harmonics and that the vec is non-empty and in order
 * of increasing frequency.
 *
 * This is a strategy to reduce the gibbs phenomenon when approximating fourier series.
 * */
inline void sigmaApproximate(std::vector<syz_SineBankWave> *waves) {
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
inline  void normalizeSeries(std::vector<syz_SineBankWave> *waves) {
  double gsum = 0.0;
  for (auto &w : *waves) {
    gsum += w.gain;
  }
  double nf = 1.0 / gsum;
  for (auto &w : *waves) {
    w.gain *= nf;
  }
}

inline  std::vector<syz_SineBankWave> buildTriangleSeries(unsigned int partials) {
  std::vector<syz_SineBankWave> out;

  double sign = 1.0;
  for (unsigned int i = 0; i < partials; i++) {
    double n = 2 * i + 1;
    out.emplace_back(syz_SineBankWave{(double)n, 0.0, sign / (n * n)});
    sign *= -1.0;
  }

  // Don't sigma approximate: it makes things much worse audibly at low partial counts.
  normalizeSeries(&out);
  return out;
}

inline std::vector<syz_SineBankWave> buildSawtoothSeries(unsigned int partials) {
  std::vector<syz_SineBankWave> out;

  // `$ x(t)=-{\frac {2}{\pi }}\sum _{k=1}^{\infty }{\frac {{\left(-1\right)}^{k}}{k}}\sin \left(2\pi kt\right) $`
  // but we move the outer negation into the sum and let our normalizer handle the multiple on the gain.

  double sign = -1.0;
  for (unsigned int i = 1; i <= partials; i++) {
    out.emplace_back(syz_SineBankWave{(double)i, 0.0, sign / (double)i});
    sign *= -1.0;
  }

  // Don't sigma approximate: it makes things much worse audibly at low partial counts.
  normalizeSeries(&out);
  return out;
}

inline std::vector<syz_SineBankWave> buildSquareSeries(unsigned int partials) {
  std::vector<syz_SineBankWave> out{};
  for (unsigned int p = 0; p < partials; p++) {
    double gain = 1.0 / (2 * p + 1);
    out.emplace_back(syz_SineBankWave{(double)(p * 2 + 1), 0.0, gain});
  }

  sigmaApproximate(&out);
  normalizeSeries(&out);
  return out;
}

} // namespace sb_construction_helpers

} // namespace synthizer
