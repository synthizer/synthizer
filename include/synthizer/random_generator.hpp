#pragma once

#include "synthizer/xoshiro.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>

namespace synthizer {

std::array<std::uint64_t, 4> static inline makeXoshiroSeed() {
  static std::atomic<int> seed_start = 0;
  int start = seed_start.fetch_add(8, std::memory_order_relaxed);
  std::array<std::uint32_t, 8> seed32;
  std::seed_seq gen{start, start + 1, start + 2, start + 3, start + 4, start + 5, start + 6, start + 6};
  gen.generate(seed32.begin(), seed32.end());
  return {(std::uint64_t(seed32[0]) << 32) | seed32[1], (std::uint64_t(seed32[2]) << 32) | seed32[3],
          (std::uint64_t(seed32[4]) << 32) | seed32[5], (std::uint64_t(seed32[6]) << 32) | seed32[7]};
}

/*
 * Converts a significand, a binary number with 23 bits, to a float between -1 and 1.
 * */
static inline float significandToFloat(unsigned int significand) {
  /* Positive. */
  const std::uint32_t sign = 0x00000000;
  /* Exponent = 1. */
  const std::uint32_t exponent = 0x40000000;
  float out;
  std::uint32_t in = sign | exponent | significand;
  std::memcpy(&out, &in, sizeof(float));
  /* Our float is between 2 and 4. We want between -1 and 1. */
  return out - 3.0f;
}

/*
 * Knows how to generate a variety of types of audio sample.  All real results are between -1 and 1.
 * */
class RandomGenerator {
public:
  RandomGenerator() : gen(makeXoshiroSeed()) {}

  float generateFloat() {
    std::uint64_t num = this->gen.next();
    return significandToFloat(num & ((2 << 22) - 1));
  }

  /*
   * Generate 4 floats, and write them to the specified output references.
   *
   * These floats are what you'd get if you converted 16-bit signed samples to floats, i.e. less precise than
   * generateFloat but still good enough for audio.  This is much faster than generating 1 float 4 times, because we can
   * get 4 floats from one run of a 64-bit random number generator, as long as we don't mind the limitation.
   *
   * We don't use a tuple here because of debug builds; we want this to be as fast
   * as we can manage.
   * */
  void generateFloat4(float &o1, float &o2, float &o3, float &o4) {
    std::uint64_t random = this->gen.next();
    std::uint16_t i1, i2, i3, i4;
    const std::uint16_t bit16 = 0xffff;
    i1 = random & bit16;
    i2 = (random >> 16) & bit16;
    i3 = (random >> 32) & bit16;
    i4 = (random >> 48) & bit16;
    o1 = 1.0f - (i1 / 32768.0f);
    o2 = 1.0f - (i2 / 32768.0f);
    o3 = 1.0f - (i3 / 32768.0f);
    o4 = 1.0f - (i4 / 32768.0f);
  }

private:
  xoshiro::Xoshiro256PlusPlus gen;
};

} // namespace synthizer
