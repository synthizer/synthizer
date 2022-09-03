#pragma once

#include "synthizer.h"

#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/noise_generator.hpp"
#include "synthizer/property_internals.hpp"

#include <memory>
#include <optional>

namespace synthizer {

class FadeDriver;

/*
 * The odd naming is so that NoiseGenerator can be the implementation, so we can reuse it for
 * i.e. reverb.
 * */
class ExposedNoiseGenerator : public Generator {
public:
  ExposedNoiseGenerator(const std::shared_ptr<Context> &ctx, unsigned int channels)
      : Generator(ctx), channels(channels) {
    for (unsigned int i = 0; i < channels; i++) {
      this->generators.emplace_back();
    }
  }

  int getObjectType() override;

  unsigned int getChannels() const override { return this->channels; }

  void generateBlock(float *out, FadeDriver *gain_driver) override;

  std::optional<double> startGeneratorLingering() override;

#define PROPERTY_CLASS ExposedNoiseGenerator
#define PROPERTY_LIST NOISE_GENERATOR_PROPERTIES
#define PROPERTY_BASE Generator
#include "synthizer/property_impl.hpp"

private:
  deferred_vector<NoiseGenerator> generators;
  unsigned int channels;
};

inline int ExposedNoiseGenerator::getObjectType() { return SYZ_OTYPE_NOISE_GENERATOR; }

inline void ExposedNoiseGenerator::generateBlock(float *out, FadeDriver *gd) {
  auto working_buf_guard = acquireBlockBuffer();
  float *working_buf_ptr = working_buf_guard;

  std::fill(working_buf_ptr, working_buf_ptr + config::BLOCK_SIZE * this->channels, 0.0f);

  int noise_type;

  if (this->acquireNoiseType(noise_type)) {
    for (auto &g : this->generators) {
      g.setNoiseType(noise_type);
    }
  }

  for (unsigned int i = 0; i < this->channels; i++) {
    this->generators[i].generateBlock(config::BLOCK_SIZE, working_buf_ptr + i, this->channels);
  }

  gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      float g = gain_cb(i);
      for (unsigned int ch = 0; ch < this->channels; ch++) {
        unsigned int ind = this->channels * i + ch;
        out[ind] += g * working_buf_ptr[ind];
      }
    }
  });
}

inline std::optional<double> ExposedNoiseGenerator::startGeneratorLingering() {
  setGain(0.0);
  /**
   * Linger for one block, to give the gain a chance to fade out.
   *
   * This is also sufficient for if the generator is paused.
   * */
  return config::BLOCK_SIZE / (double)config::SR;
}

} // namespace synthizer
