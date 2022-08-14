#pragma once

#include "synthizer/data/arrays.hpp"

#include <algorithm>
#include <cassert>
#include <climits>

namespace synthizer {

/*
 * Helper functions for getting prime numbers.
 * */

/*
 * All of the functions below need to binary search the set of primes in this fashion.
 *
 * Returns the index of the first prime above the specified value or, if the value is bigger than the array,
 * the index of the end of the array.
 * */
inline std::size_t primeIndex(unsigned int input) {
  auto res = std::upper_bound(data::primes.data(), data::primes.data() + data::primes.size(), input);
  return res - data::primes.data();
}

/*
 * The maximum prime this module can ever return.
 * */
inline unsigned int getMaxPrime() { return data::primes[data::primes.size() - 1]; }

struct PrimesSetDef {
  unsigned int count;
  unsigned int *values;
};

inline bool primesSetContains(const PrimesSetDef &set, unsigned int value) {
  for (unsigned int i = 0; i < set.count; i++) {
    if (set.values[i] == value) {
      return true;
    }
  }
  return false;
}

/*
 * get the closest prime to a specified integer, excluding primes in the specified set.
 *
 * This is used primarily by reverb, in order to avoid duplicate primes.
 * */
inline unsigned int getClosestPrimeRestricted(unsigned int input, unsigned int set_size, unsigned int *set_values) {
  PrimesSetDef set = {set_size, set_values};

  /*
   * We generate 2 candidates, move them until they're both not in the set, and then take
   * the one with the least error to the specified value.
   * */
  unsigned int upper_index = primeIndex(input);
  unsigned int lower_index = upper_index != 0 ? upper_index - 1 : 0;

  /*
   * Move the indices until they either hit the ends of the array, or are outside the set.
   * */
  for (; lower_index != 0 && primesSetContains(set, data::primes[lower_index]); lower_index--)
    ;
  for (; upper_index != data::primes.size() && primesSetContains(set, data::primes[upper_index]); upper_index++)
    ;

  /*
   * This is technically possible, but nothing in Synthizer asks for enough primes that we should ever be able
   * to fully exhaust the array. Let's hard crash to make it obvious that something went wrong.
   * */
  assert(lower_index != 0 || upper_index != data::primes.size());

  unsigned int lower_error = UINT_MAX;
  unsigned int upper_error = UINT_MAX;
  if (lower_index != 0) {
    lower_error = input - data::primes[lower_index];
  }
  if (upper_index != data::primes.size()) {
    upper_error = data::primes[upper_index] - input;
  }

  /*
   * One of the indices has to be valid, and it has to be less than UINT_MAX because the primes array doesn't go that
   * high. If one of them is invalid, the error will be UINT_MAX, and thus greater than the valid one.
   * */
  if (lower_error < upper_error) {
    return data::primes[lower_index];
  }
  return data::primes[upper_index];
}

/*
 * get the closest prime number to a specified integer.
 * */
inline unsigned int getClosestPrime(unsigned int input) { return getClosestPrimeRestricted(input, 0, nullptr); }

} // namespace synthizer