#pragma once

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/block_delay_line.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/types.hpp"

#include <concurrentqueue.h>
#include <pdqsort.h>

#include <algorithm>
#include <array>
#include <utility>

namespace synthizer {

class Context;

struct EchoTapConfig {
  float gain_l = 0.5, gain_r = 0.5;
  unsigned int delay = 0;
};

/* For later. This will probably eventually hold, i.e., crossfading state. */
struct EchoTap {
  EchoTapConfig config;
};

constexpr unsigned int ECHO_MAX_DELAY = nextMultipleOf(config::SR * 5, config::BLOCK_SIZE);

/*
 * Shim base class for the C API, so that it can work for both global and generator-specific echoes.
 * */
class EchoEffectCInterface {
public:
  virtual void pushNewConfig(deferred_vector<EchoTapConfig> &&config) = 0;
};

/*
 * A stereo echo effect with variable taps. This gets to use the "echo" name because it is broadly speaking the most
 * common form of echo.
 *
 * Right now the delay line length is hardcoded to approximately 5 seconds: this is because we've yet to need a
 * dynamically sized delay line, so there's not currently one implemented. I'm going to circle back on this later, but
 * for the sake of early access, let's get this out the door without it.
 * */
template <typename BASE> class EchoEffect : public BASE, public EchoEffectCInterface {
public:
  EchoEffect(const std::shared_ptr<Context> &ctx) : BASE(ctx, 1) {}

  void runEffect(unsigned int block_time, unsigned int input_channels, float *input, unsigned int output_channels,
                 float *output, float gain) override;
  void resetEffect() override;

  void pushNewConfig(deferred_vector<EchoTapConfig> &&config) override;

  double getEffectLingerTimeout() override;

private:
  /*
   * There's actually 4 entirely distinct scenarios, both of the following in any combination:
   * 1. We just got new config (FADE_IN=true); and
   * 2. We need to detour through an intermediate buffer to remix.
   *
   * 1 changes at most once per config update and 2 is fixed for the lifetime of the context, so get the compiler to
   * help us out: generate 4 versions, and rely on the fact that we never change which version we use to make up for
   * code cache unfriendliness.
   *
   * Assumes that the audio is already in the delay line, and that the parent will handle channel mixing on the way out.
   * */
  template <bool FADE_IN, bool ADD> void runEffectInternal(float *output, float gain);

  BlockDelayLine<2, ECHO_MAX_DELAY / config::BLOCK_SIZE> line;
  /*
   * Configurations are pushed onto this queue, then picked up the next time runEffect is called.
   *
   * This will probably be replaced with a box (see boxes.hpp) once we have a suitable one. That's on the to-do list.
   * */
  moodycamel::ConcurrentQueue<deferred_vector<EchoTapConfig>> pending_configs;
  deferred_vector<EchoTap> taps;
  /* What we need to pass to runRead Loop. */
  unsigned int max_delay_tap = 0;
};

template <typename BASE>
template <bool FADE_IN, bool ADD>
void EchoEffect<BASE>::runEffectInternal(float *output, float gain) {
  this->line.runReadLoop(this->max_delay_tap, [&](unsigned int i, auto &reader) {
    float acc_l = 0.0f;
    float acc_r = 0.0f;
    for (auto &t : this->taps) {
      acc_l += reader.read(0, t.config.delay) * t.config.gain_l;
      acc_r += reader.read(1, t.config.delay) * t.config.gain_r;
    }
    if (FADE_IN) {
      float g = i / (float)config::BLOCK_SIZE;
      acc_l *= g;
      acc_r *= g;
    }
    acc_l *= gain;
    acc_r *= gain;
    if (ADD) {
      output[i * 2] += acc_l;
      output[i * 2 + 1] += acc_r;
    } else {
      output[i * 2] = acc_l;
      output[i * 2 + 1] = acc_r;
    }
  });
}

template <typename BASE>
void EchoEffect<BASE>::runEffect(unsigned int block_time, unsigned int input_channels, float *input,
                                 unsigned int output_channels, float *output, float gain) {
  (void)block_time;

  /* Always 2 channels. */
  auto working_buf_guard = acquireBlockBuffer();
  float *working_buf = working_buf_guard;

  auto *buffer = this->line.getNextBlock();
  std::fill(buffer, buffer + config::BLOCK_SIZE * 2, 0.0f);
  /* Mix data to stereo. */
  mixChannels(config::BLOCK_SIZE, input, input_channels, buffer, 2);

  /* maybe apply the new config. */
  deferred_vector<EchoTapConfig> new_config;
  bool will_crossfade = this->pending_configs.try_dequeue(new_config);
  /* Drain any stale values. When this is moved to a proper box, this will be handled for us. */
  while (this->pending_configs.try_dequeue(new_config))
    ;
  if (will_crossfade) {
    this->taps.clear();
    this->max_delay_tap = 0;
    for (auto &c : new_config) {
      EchoTap tap;
      tap.config = c;
      this->taps.push_back(tap);
      this->max_delay_tap = this->max_delay_tap > c.delay ? this->max_delay_tap : c.delay;
    }
  }

  if (output_channels != 2) {
    if (will_crossfade) {
      this->runEffectInternal<true, false>(&working_buf[0], gain);
    } else {
      this->runEffectInternal<false, false>(&working_buf[0], gain);
    }
    mixChannels(config::BLOCK_SIZE, &working_buf[0], 2, output, output_channels);
  } else {
    if (will_crossfade) {
      this->runEffectInternal<true, true>(output, gain);
    } else {
      this->runEffectInternal<false, true>(output, gain);
    }
  }
}

template <typename BASE> void EchoEffect<BASE>::resetEffect() { this->line.clear(); }

template <typename BASE> void EchoEffect<BASE>::pushNewConfig(deferred_vector<EchoTapConfig> &&config) {
  pdqsort_branchless(config.begin(), config.end(), [](auto &a, auto &b) { return a.delay < b.delay; });
  if (this->pending_configs.enqueue(std::move(config)) == false) {
    throw std::bad_alloc();
  }
}

template <typename BASE> double EchoEffect<BASE>::getEffectLingerTimeout() {
  /**
   * The best we can do is linger until the entire delay line is empty.
   *
   * Add a block to prevent rounding error.
   * */
  return (ECHO_MAX_DELAY / config::BLOCK_SIZE + 1) * config::BLOCK_SIZE / (double)config::SR;
}

} // namespace synthizer
