#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>

namespace synthizer {
const double PI = 3.141592653589793;
const float PIF = (float)PI;

/* WebAudio definition, also field quantity. */
inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }
inline double gainToDb(double gain) { return 20 * std::log10(gain); }

template <typename T> T clamp(T v, T min, T max) {
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

/**
 * Compute floor(input/denom) where denom is a power of 2.
 *
 * This is used in some contexts to simulate decimal arithmetic with integers where fp error is unsuitable.  Though this
 * is in effect bitmasking, it is nonetheless helpful to clearly be able to see what's going on, and to have assertions
 * that it's actually going to work.
 * */
constexpr std::uint64_t floorByPowerOfTwo(std::uint64_t input, std::uint64_t denom) {
  assert(denom != 0);

  // check that denom is a power of 2.
  std::uint64_t dencheck = denom & (denom - 1);
  (void)dencheck;
  assert(dencheck == 0);

  return input & ~(denom - 1);
}

constexpr std::uint64_t ceilByPowerOfTwo(std::uint64_t input, std::uint64_t denom) {
  return floorByPowerOfTwo(input + denom - 1, denom);
}

} // namespace synthizer