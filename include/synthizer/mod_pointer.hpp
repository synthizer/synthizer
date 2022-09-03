/**
 * An abstraction that lets us work with data where it is sometimes in a form that requires modulus, and sometimes in a
 * form that can be accessed directly.
 *
 * ModPointer is a typedef to a variant for use with std::visit, which is either an object that acts like a pointer
 * while transparently performing modulus, or a raw pointer itself.  This can then be used either directly by writing
 * template functions or indirectly by writing overloads which capture pointers to some type T and doing whatever with
 * them.
 *
 * In debug builds, the raw pointers can be replaced with an asserting version that can detect bounds checks.  Because
 * some things actually hard rely on the raw pointer being available, this is off by default and must be passed as an
 * additional parameter to the various createXXX functions below.  In a release build (specifically when NDEBUG is
 * defined), this is a no-op.
 * */
#pragma once

#include "synthizer/compiler_specifics.hpp"

#include <cassert>
#include <cstdint>
#include <variant>

namespace synthizer {

namespace mod_pointer_detail {
template <std::size_t LEN> class StaticModProvider {
public:
  std::size_t getLength() const { return LEN; }
  std::size_t doMod(std::size_t val) const { return val % LEN; }
};

class DynamicModProvider {
public:
  DynamicModProvider(std::size_t len) : length(len) {}

  std::size_t getLength() const { return this->length; }
  std::size_t doMod(std::size_t val) const { return val % this->length; }

private:
  std::size_t length;
};

/**
 * A static modulus provider which asserts that modulus never happens.
 * */
template <std::size_t LEN> class StaticAssertingProvider {
public:
  std::size_t getLength() const { return LEN; }
  std::size_t doMod(std::size_t val) const {
    assert(val < LEN);
    return val;
  }
};

/**
 * A dynamic modulus provider which asserts that modulus never happens.
 * */
class DynamicAssertingProvider {
public:
  DynamicAssertingProvider(std::size_t _len) : length(_len) {}

  std::size_t getLength() const { return this->length; }
  std::size_t doMod(std::size_t val) const {
    assert(val < this->length);
    return val;
  }

private:
  std::size_t length = 0;
};
} // namespace mod_pointer_detail

/**
 * The modulating variant payload.
 *
 * This type wraps around the innere data, performing modulus as needed.
 *
 * There is one important invariant: no movement of the pointer is permitted to wrap entirely around the space.  That
 * is, any increment or decrement may not be by more than LEN, and any index operation may not be with an index outside
 * the range 0 <= index < LEN.  Indices of LEN are the same as 0, by the rules of modular arithmetic.
 *
 * This is, in essence, a "pointer", and being outside the range is the same as out of bounds access.  One difference
 * which hopefully never matters is that, for our sanity, we don't support negative indices.
 * */
template <typename T, typename MOD_PROVIDER_T> class ModSlice {
public:
  ModSlice(T *_data, std::size_t initial_offset, MOD_PROVIDER_T mod_provider);

  T &operator[](std::size_t index) const;
  T &operator*() const;
  T *operator->() const;

  ModSlice<T, MOD_PROVIDER_T> &operator++();
  ModSlice<T, MOD_PROVIDER_T> &operator--();
  ModSlice<T, MOD_PROVIDER_T> operator++(int);
  ModSlice<T, MOD_PROVIDER_T> operator--(int);
  ModSlice<T, MOD_PROVIDER_T> operator+(std::size_t increment) const;
  ModSlice<T, MOD_PROVIDER_T> operator-(std::size_t decrement) const;
  void operator+=(std::size_t increment);

private:
  /**
   *Perform (offset +  increment)%LEN in the forward direction.
   */
  std::size_t addIndexRelative(std::size_t increment) const;

  /**
   * Compute (LEN + offset - decrement) % LEN, to subtract without underflow.
   * */
  std::size_t subIndexRelative(std::size_t decrement) const;

  T *data;
  std::size_t offset;
  MOD_PROVIDER_T mod_provider;
};

/**
 * A StaticModPointer is a variant of pointer-like objects to a slice of memory at some offset, where access to the
 * object may need mod, and the length of the underlying buffer is known at compile time.
 *
 * The idea here is that when reading from a delay line for example, if we know how far ahead we're going to read, we
 * may be able to return a raw pointer.  Likewise for powers of 2, and so on.
 *
 * While the non-pointer variants aren't guaranteed to be anything in particular save that they offer a pointer-like
 * interface, there will always be a T* variant and it will always be used when no modulus is required.  See the below
 * helper function for construction of ModPointers.
 *
 * IMPLEMENTATION NOTE: the instrumented debug variant must be last in order to allow tests to reliably succeed on all
 * platforms.
 * */
template <typename T, std::size_t LEN>
using StaticModPointer = std::variant<ModSlice<T, mod_pointer_detail::StaticModProvider<LEN>>, T *,
#ifndef NDEBUG
                                      ModSlice<T, mod_pointer_detail::StaticAssertingProvider<LEN>>
#endif
                                      >;

/**
 * The same as StaticModPointer, but the modulus case comes from a runtime value.
 *
 * This is significantly more expensive in most contexts.
 *
 * IMPLEMENTATION NOTE: the debug instrumentation variant must be last so tests can reliably pass on all platforms.
 * */
template <typename T>
using DynamicModPointer = std::variant<ModSlice<T, mod_pointer_detail::DynamicModProvider>, T *,
#ifndef NDEBUG
                                       ModSlice<T, mod_pointer_detail::DynamicAssertingProvider>
#endif
                                       >;

/**
 * Given a pointer, an offset, and one past the maximum index which may be accessed, return a ModPointer that can handle
 * it.
 * */
template <typename T, std::size_t LEN>
inline StaticModPointer<T, LEN> createStaticModPointer(T *data, std::size_t offset, std::size_t slice_len,
                                                       bool allow_asserting_provider = false);

template <typename T>
inline DynamicModPointer<T> createDynamicModPointer(T *data, std::size_t offset, std::size_t max_index,
                                                    std::size_t buffer_len, bool allow_asserting_provider = false);

template <typename T, typename MOD_PROVIDER_T>
ModSlice<T, MOD_PROVIDER_T>::ModSlice(T *_data, std::size_t initial_offset, MOD_PROVIDER_T _length_provider)
    : data(_data), offset(initial_offset), mod_provider(_length_provider) {}

template <typename T, typename MOD_PROVIDER_T>
FLATTENED std::size_t ModSlice<T, MOD_PROVIDER_T>::addIndexRelative(std::size_t increment) const {
  return this->mod_provider.doMod(this->offset + increment);
}

template <typename T, typename MOD_PROVIDER_T>
FLATTENED std::size_t ModSlice<T, MOD_PROVIDER_T>::subIndexRelative(std::size_t decrement) const {
  assert(decrement <= this->mod_provider.getLength());
  return this->mod_provider.doMod(this->mod_provider.getLength() + this->offset - decrement);
}

template <typename T, typename MOD_PROVIDER_T>
FLATTENED T &ModSlice<T, MOD_PROVIDER_T>::operator[](std::size_t index) const {
  auto actual_index = this->addIndexRelative(index);
  return this->data[actual_index];
}

template <typename T, typename MOD_PROVIDER_T> FLATTENED T &ModSlice<T, MOD_PROVIDER_T>::operator*() const {
  return (*this)[0];
}

template <typename T, typename MOD_PROVIDER_T> T *ModSlice<T, MOD_PROVIDER_T>::operator->() const {
  return &((*this)[0]);
}

template <typename T, typename MOD_PROVIDER_T> ModSlice<T, MOD_PROVIDER_T> &ModSlice<T, MOD_PROVIDER_T>::operator++() {
  this->offset = this->addIndexRelative(1);
  return *this;
}

template <typename T, typename MOD_PROVIDER_T> ModSlice<T, MOD_PROVIDER_T> &ModSlice<T, MOD_PROVIDER_T>::operator--() {
  // This is equivalent to going around one minus the len.
  this->offset = this->subIndexRelative(1);
  return *this;
}

template <typename T, typename MOD_PROVIDER_T>
ModSlice<T, MOD_PROVIDER_T> ModSlice<T, MOD_PROVIDER_T>::operator++(int) {
  auto old = *this;
  ++(*this);
  return old;
}

template <typename T, typename MOD_PROVIDER_T>
ModSlice<T, MOD_PROVIDER_T> ModSlice<T, MOD_PROVIDER_T>::operator--(int) {
  auto old = *this;
  --(*this);
  return old;
}

template <typename T, typename MOD_PROVIDER_T>
ModSlice<T, MOD_PROVIDER_T> ModSlice<T, MOD_PROVIDER_T>::operator+(std::size_t increment) const {
  auto copy = *this;
  copy.offset = this->addIndexRelative(increment);
  return copy;
}

template <typename T, typename MOD_PROVIDER_T>
ModSlice<T, MOD_PROVIDER_T> ModSlice<T, MOD_PROVIDER_T>::operator-(std::size_t decrement) const {
  auto copy = *this;
  copy.offset = this->subIndexRelative(decrement);
  return copy;
}

template <typename T, typename MOD_PROVIDER_T>
FLATTENED void ModSlice<T, MOD_PROVIDER_T>::operator+=(std::size_t increment) {
  *this = *this + increment;
}

template <typename T, std::size_t LEN>
FLATTENED StaticModPointer<T, LEN> createStaticModPointer(T *data, std::size_t offset, std::size_t slice_len,
                                                          bool allow_asserting_provider) {
  (void)allow_asserting_provider; // because this isn't referenced in release builds.

  if (offset + slice_len > LEN) {
    return ModSlice<T, mod_pointer_detail::StaticModProvider<LEN>>{data, offset,
                                                                   mod_pointer_detail::StaticModProvider<LEN>{}};
  } else {
#ifndef NDEBUG
    if (allow_asserting_provider == true) {
      return ModSlice<T, mod_pointer_detail::StaticAssertingProvider<LEN>>{
          data, offset, mod_pointer_detail::StaticAssertingProvider<LEN>{}};
    }
#endif

    return data + offset;
  }
}

template <typename T>
FLATTENED DynamicModPointer<T> createDynamicModPointer(T *data, std::size_t offset, std::size_t slice_len,
                                                       std::size_t buffer_len, bool allow_asserting_provider) {
  (void)allow_asserting_provider; // because this isn't referenced in release builds.

  if (offset + slice_len > buffer_len) {
    return ModSlice<T, mod_pointer_detail::DynamicModProvider>{data, offset,
                                                               mod_pointer_detail::DynamicModProvider{buffer_len}};
  } else {
#ifndef NDEBUG
    if (allow_asserting_provider) {
      return ModSlice<T, mod_pointer_detail::DynamicAssertingProvider>{
          data, offset, mod_pointer_detail::DynamicAssertingProvider{buffer_len}};
    }
#endif
    return data + offset;
  }
}

} // namespace synthizer
