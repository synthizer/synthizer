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

  unsigned int getChannels() override { return this->channels; }

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

} // namespace synthizer
