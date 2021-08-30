#pragma once

#include <algorithm>

#include "synthizer/types.hpp"

#include <array>
#include <cassert>

namespace synthizer {

/*
 *A simple bitset of arbitrary but fixed size.
 */
template <unsigned int BITS> class Bitset {
public:
  static const unsigned int SIZE = BITS;
  static const unsigned int SIZE_IN_BYTES = nextMultipleOf(BITS, 8) / 8;

  bool get(unsigned int index) {
    assert(index < BITS);
    unsigned int byte = this->data[index / 8];
    unsigned int bit = index % 8;
    return (byte & (1 << bit)) != 0;
  }

  void set(unsigned int index, bool value) {
    assert(index < SIZE);
    unsigned int byte = index / 8;
    unsigned int bit = index % 8;
    unsigned int mask = (1 << bit);
    this->data[byte] = value ? this->data[byte] | mask : this->data[byte] & ~mask;
  }

  unsigned int getBitCount() {
    unsigned int ret = 0;
    for (unsigned int i = 0; i < SIZE_IN_BYTES; i++)
      ret += __builtin_popcount(this->data[i]);
    return ret;
  }

  /* Returns SIZE or greater if no bit is available. Will not necessarily return SIZE. */
  unsigned int getFirstUnsetBit() {
    for (unsigned int i = 0; i < SIZE_IN_BYTES; i++) {
      unsigned char d = data[i];
      if (d == 0) {
        return i * 8;
      }
      if (d == 255)
        continue; // no fre bit.
      return i * 8 + __builtin_ctz(~(unsigned int)d);
    }
    return SIZE;
  }

  void setAll(bool value) {
    unsigned char x = value ? ~(unsigned char)0 : 0;
    std::fill(this->data.begin(), this->data.end(), x);
  }

private:
  std::array<unsigned char, SIZE_IN_BYTES> data;
};

} // namespace synthizer