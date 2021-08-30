#pragma once

#include "synthizer/buffer.hpp"
#include "synthizer/edge_trigger.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include <memory>
#include <optional>

namespace synthizer {

class FadeDriver;

class BufferGenerator : public Generator {
public:
  BufferGenerator(std::shared_ptr<Context> ctx);

  int getObjectType() override;
  unsigned int getChannels() override;
  void generateBlock(float *output, FadeDriver *gain_driver) override;

  std::optional<double> startGeneratorLingering() override;

#define PROPERTY_CLASS BufferGenerator
#define PROPERTY_BASE Generator
#define PROPERTY_LIST BUFFER_GENERATOR_PROPERTIES
#include "synthizer/property_impl.hpp"

private:
  template <bool L> void readInterpolated(double pos, float *out, float gain);
  /* Adds to destination, per the generators API. */
  void generateNoPitchBend(float *out, FadeDriver *gain_driver);
  template <bool L> void generatePitchBendHelper(float *out, FadeDriver *gain_driver, double pitch_bend);
  void generatePitchBend(float *out, FadeDriver *gain_driver, double pitch_bend);
  void configureBufferReader(const std::shared_ptr<Buffer> &b);
  /**
   * Either sends finished or looped, depending.
   * */
  void handleEndEvent();

  BufferReader reader;
  /**
   * Counters for events.
   * */
  unsigned int finished_count = 0, looped_count = 0;
  bool sent_finished = false;
  double position_in_samples = 0.0;
};

} // namespace synthizer
