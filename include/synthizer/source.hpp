#pragma once

#include "synthizer_constants.h"

#include "synthizer/base_object.hpp"
#include "synthizer/config.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/faders.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/panning/panner.hpp"
#include "synthizer/pausable.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/routable.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace synthizer {

/* Implementation of a variety of sources. */

class BiquadFilter;
class Context;

class Source : public RouteOutput, public Pausable {
public:
  Source(std::shared_ptr<Context> ctx) : RouteOutput(ctx) { this->setGain(1.0); }

  virtual ~Source() {}

  virtual void tickAutomation() override;

  /*
   * called before running, to give subclasses a chance to set gain_3d.
   *
   * Called in fillBlock since this base class doesn't have a run.
   * */
  virtual void preRun() {}

  /* Set the 3d gain, an additional gain aplied on top of the gain property. */
  void setGain3D(double gain);

  /* Should write to appropriate places in the context on its own. */
  virtual void run(unsigned int out_channels, float *out) = 0;

  /* Weak add and/or remove a generator from this source. */
  virtual void addGenerator(std::shared_ptr<Generator> &gen);
  virtual void removeGenerator(std::shared_ptr<Generator> &gen);
  bool hasGenerator(std::shared_ptr<Generator> &generator);

  bool wantsLinger() override;
  std::optional<double> startLingering(const std::shared_ptr<CExposable> &reference,
                                       double configured_timeout) override;

#define PROPERTY_CLASS Source
#define PROPERTY_BASE BaseObject
#define PROPERTY_LIST SOURCE_PROPERTIES
#include "synthizer/property_impl.hpp"

protected:
  /*
   * An internal buffer, which accumulates all generators.
   * */
  std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> block;

  /*
   * Fill the internal block, downmixing and/or upmixing generators
   * to the specified channel count.
   *
   * Also pumps the router, which will feed effects. We do this here so that all source types just work for free.
   * */
  void fillBlock(unsigned int channels);

private:
  deferred_vector<GeneratorRef> generators;
  FadeDriver gain_fader = {1.0f, 1};
  /* Used to detect channel changes in fillBlock. */
  unsigned int last_channels = 0;
  std::shared_ptr<BiquadFilter> filter = nullptr, filter_direct = nullptr, filter_effects = nullptr;
  bool is_lingering = false;
  /**
   * A countdown for lingering. Decremented for every block after there are no generators. When it hits zero,
   * the source enqueues itself for death.
   *
   * This exists because some source/panner types need a few blocks of audio to finish fading out, as do the
   * filters. Without it, sources can click on death.
   * */
  unsigned int linger_countdown = 3;

  /* gains used by 3D sources, applied to the block on top of whatever the user set. */
  double gain_3d = 1.0;
  bool gain_3d_changed = false;
};

} // namespace synthizer
