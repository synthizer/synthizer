#pragma once

#include "synthizer.h"

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

} // namespace synthizer
