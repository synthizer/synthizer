#pragma once

#include <array>
#include <cassert>
#include <type_traits>
#include <utility>

#include "synthizer/compiler_specifics.hpp"
#include "synthizer/config.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * This file implements a delay line which allows for delays of up to N blocks, and contains special helper methods to
 * avoid modulus in some cases. The data is allocated statically.
 *
 * Primarily, this is of use to the HRTF implementation which uses BlockDelayLine<8, 2> to avoid modulus on most blocks,
 * and will probably also be used by FDN reverberation for similar reasons
 *
 * This is opaque, but there are two usage patterns:
 *
 * The per-block method:
 * - Something calls line.getNextBlock(), which returns a pointer to the next block of data.
 * - Something fills the next block of data.
 * - Then call runReadLoop, with a templated function that gets passed an object with read(channel, delay).
 *
 * The per-sample pattern:
 * - Something calls runRWLoop with a closure which gets passed something with read as above, but also write(unsigned
 * int channel, float value).
 *
 * In the first pattern, and in the case that the closure writes before reading in the second pattern, 0 delay is
 * possible.
 *
 * Finally, for advanced users, readFrame and writeFrame exist in the appropriate cases and will return pointers to the
 * buffer itself.
 * */

template <unsigned int LANES, unsigned int SIZE_IN_BLOCKS> class BlockDelayLine {
  std::array<float, config::BLOCK_SIZE *SIZE_IN_BLOCKS *LANES> data = {0.0f};

public:
  BlockDelayLine() {}

  static unsigned int modulusIndexProducer(unsigned int delay, unsigned int current_frame) {
    assert(delay < config::BLOCK_SIZE * SIZE_IN_BLOCKS);

    return (current_frame + config::BLOCK_SIZE * SIZE_IN_BLOCKS - delay) % (config::BLOCK_SIZE * SIZE_IN_BLOCKS);
  }

  static unsigned int identityIndexProducer(unsigned int delay, unsigned int current_frame) {
    assert(delay < config::BLOCK_SIZE * SIZE_IN_BLOCKS);
    assert(delay <= current_frame);
    return current_frame - delay;
  }

  template <unsigned int (*IND_PRODUCER)(unsigned int, unsigned int), bool WRITE_ENABLED> class Reader {
  private:
    std::array<float, LANES * SIZE_IN_BLOCKS * config::BLOCK_SIZE> &source;
    unsigned int current_frame;

  public:
    Reader(std::array<float, LANES * SIZE_IN_BLOCKS * config::BLOCK_SIZE> &source, unsigned int current_frame)
        : source(source), current_frame(current_frame) {}
    Reader(const Reader &) = delete;
    Reader(const Reader &&) = delete;

    float read(unsigned int channel, unsigned int delay) {
      assert(channel < LANES);
      unsigned int index = IND_PRODUCER(delay, this->current_frame);
      return this->source[index * LANES + channel];
    }

    float *readFrame(unsigned int delay) {
      auto ind = IND_PRODUCER(delay, this->current_frame);
      return &this->source[ind * LANES];
    }

    template <typename RET = void>
    auto write(unsigned int channel, float value) -> std::enable_if_t<WRITE_ENABLED, RET> {
      assert(channel < LANES);
      this->source[this->current_frame * LANES + channel] = value;
    }

    template <typename RET = float *> auto writeFrame() -> std::enable_if_t<WRITE_ENABLED, RET> {
      return &this->source[this->current_frame * LANES];
    }

    template <typename FUNCTOR> FLATTENED void runLoop(unsigned int done, unsigned int samples, FUNCTOR &&f) {
      for (unsigned int i = 0; i < samples; i++) {
        f(done + i, *this);
        this->current_frame++;
      }
    }
  };

  template <typename F> FLATTENED void runReadLoop(unsigned int max_delay, F &&closure) {
    return runLoopPrivSplit<F, F, false>(max_delay, config::BLOCK_SIZE, closure, 0, closure);
  }

  template <typename F> FLATTENED void runRwLoop(unsigned int max_delay, F &&f) {
    return runLoopPrivSplit<F, F, true>(max_delay, config::BLOCK_SIZE, f, 0, f);
  }

  template <typename F1, typename F2>
  FLATTENED void runReadLoopSplit(unsigned int max_delay, unsigned int first, F1 &&first_c, unsigned int second,
                                  F2 &&second_c) {
    return this->runLoopPrivSplit<F1, F2, false>(max_delay, first, first_c, second, second_c);
  }

  template <typename F1, typename F2>
  FLATTENED void runRwLoopSplit(unsigned int max_delay, unsigned int first, F1 &&first_c, unsigned int second,
                                F2 &&second_c) {
    return this->runLoopPrivSplit<F1, F2, true>(max_delay, first, first_c, second, second_c);
  }

  float *getNextBlock() {
    assert(this->current_frame % config::BLOCK_SIZE == 0);
    assert(this->current_frame < this->data.size());

    auto ret = &this->data[this->current_frame * LANES];
    return ret;
  }

  void clearChannel(unsigned int channel) {
    assert(channel < LANES);
    for (unsigned int i = channel; i < this->data.size(); i += LANES) {
      this->data[i] = 0.0f;
    }
  }

  void clear() { std::fill(this->data.begin(), this->data.end(), 0.0f); }

private:
  template <typename F1, typename F2, bool WRITE_ENABLED>
  FLATTENED void runLoopPrivSplit(unsigned int max_delay, unsigned first, F1 &first_c, unsigned int second,
                                  F2 &second_c) {
    assert(first + second == config::BLOCK_SIZE);
    bool needs_modulus = max_delay > this->current_frame;

    if (needs_modulus) {
      Reader<modulusIndexProducer, WRITE_ENABLED> r{this->data, this->current_frame};
      r.runLoop(0, first, first_c);
      if (second)
        r.runLoop(first, second, second_c);
    } else {
      Reader<identityIndexProducer, WRITE_ENABLED> r{this->data, this->current_frame};
      r.runLoop(0, first, first_c);
      if (second)
        r.runLoop(first, second, second_c);
    }
    this->current_frame = (this->current_frame + config::BLOCK_SIZE) % (config::BLOCK_SIZE * SIZE_IN_BLOCKS);
  }

  unsigned int current_frame = 0;
};

} // namespace synthizer