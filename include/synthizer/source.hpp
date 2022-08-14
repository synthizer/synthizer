#pragma once

#include "synthizer_constants.h"

#include "synthizer/base_object.hpp"
#include "synthizer/biquad.hpp"
#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
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
#include "synthizer/vector_helpers.hpp"

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

inline void Source::addGenerator(std::shared_ptr<Generator> &generator) {
  if (this->hasGenerator(generator))
    return;
  this->generators.emplace_back(generator);
}

inline void Source::removeGenerator(std::shared_ptr<Generator> &generator) {
  if (this->generators.empty())
    return;
  if (this->hasGenerator(generator) == false)
    return;

  unsigned int index = 0;
  for (; index < this->generators.size(); index++) {
    auto s = this->generators[index].lock();
    if (s == generator)
      break;
  }

  std::swap(this->generators[this->generators.size() - 1], this->generators[index]);
  this->generators.resize(this->generators.size() - 1);
}

inline bool Source::hasGenerator(std::shared_ptr<Generator> &generator) {
  return weak_vector::contains(this->generators, generator);
}

inline void Source::tickAutomation() {
  if (this->isPaused()) {
    return;
  }

  BaseObject::tickAutomation();

  weak_vector::iterate_removing(this->generators, [](auto &g) { g->tickAutomation(); });
}

inline void Source::setGain3D(double gain) {
  this->gain_3d = gain;
  this->gain_3d_changed = true;
}

inline void Source::fillBlock(unsigned int channels) {
  this->preRun();

  auto premix_guard = acquireBlockBuffer();
  float *premix = premix_guard;
  double gain_prop;
  auto time = this->context->getBlockTime();

  if (channels != this->last_channels) {
    this->filter = createBiquadFilter(channels);
    this->filter_direct = createBiquadFilter(channels);
    this->filter_effects = createBiquadFilter(channels);
    this->filter->configure(this->getFilter());
    this->filter_direct->configure(this->getFilterDirect());
    this->filter_effects->configure(this->getFilterEffects());
    this->last_channels = channels;
  }

  if (this->acquireGain(gain_prop) || this->shouldIncorporatePausableGain() || this->gain_3d_changed) {
    gain_prop *= this->getPausableGain() * this->gain_3d;
    this->gain_3d_changed = false;
    this->gain_fader.setValue(time, gain_prop);
  }

  std::fill(&this->block[0], &this->block[0] + channels * config::BLOCK_SIZE, 0.0f);

  /**
   * There is room for further optimization here, by communicating that this block is potentially zeros
   * to the derived source classes. We'll do that later, when we also have tracking of generator
   * silence and other scheduling related functionality that makes it advantageous for the other sources to drop their
   * panners and so on.
   * */
  if (this->isPaused()) {
    return;
  }
  this->tickPausable();

  /* iterate and remove as we go to avoid locking weak_ptr twice. */
  weak_vector::iterate_removing(this->generators, [&](auto &g) {
    unsigned int nch = g->getChannels();

    if (nch == 0) {
      return;
    }

    if (nch == channels) {
      g->run(&this->block[0]);
    } else {
      std::fill(premix, premix + config::BLOCK_SIZE * nch, 0.0f);
      g->run(premix);
      mixChannels(config::BLOCK_SIZE, premix, nch, &this->block[0], channels);
    }
  });

  this->gain_fader.drive(time, [&](auto &gain_cb) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      float g = gain_cb(i);
      for (unsigned int ch = 0; ch < channels; ch++) {
        this->block[i * channels + ch] *= g;
      }
    }
  });

  struct syz_BiquadConfig filter_cfg;
  if (this->acquireFilter(filter_cfg)) {
    this->filter->configure(filter_cfg);
  }
  if (this->acquireFilterDirect(filter_cfg)) {
    this->filter_direct->configure(filter_cfg);
  }
  if (this->acquireFilterEffects(filter_cfg)) {
    this->filter_effects->configure(filter_cfg);
  }

  /*
   * Filter goes to both paths, so run it first.
   * */
  this->filter->processBlock(&this->block[0], &this->block[0], false);

  {
    auto buf_guard = acquireBlockBuffer(false);
    this->filter_effects->processBlock(&this->block[0], buf_guard, false);
    this->getOutputHandle()->routeAudio(buf_guard, channels);
  }

  /*
   * And now the direct filter, which applies only to the source's local block.
   * */
  this->filter_direct->processBlock(&this->block[0], &this->block[0], false);

  if (this->is_lingering && this->generators.empty()) {
    this->linger_countdown--;
    if (this->linger_countdown == 0) {
      this->signalLingerStopPoint();
    }
  }
}

inline bool Source::wantsLinger() { return true; }

inline std::optional<double> Source::startLingering(const std::shared_ptr<CExposable> &reference,
                                                    double configured_timeout) {
  CExposable::startLingering(reference, configured_timeout);

  this->is_lingering = true;
  /**
   * Sources linger until they have no generators.
   * */
  return std::nullopt;
}

} // namespace synthizer
