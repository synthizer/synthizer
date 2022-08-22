/**
 * An abstraction that lets us work with data where it is sometimes in a form that requires modulus, and sometimes in a
 * form that can be accessed directly.
 *
 * ModPointer is a typedef to a variant for use with std::visit, which is either an object that acts like a pointer
 * while transparently performing modulus, or a raw pointer itself.  This can then be used either directly by writing
 * template functions or indirectly by writing overloads which capture pointers to some type T and doing whatever with
 * them.
 * */
#pragma once

#include "synthizer/compiler_specifics.hpp"

#include <cassert>
#include <cstdint>
#include <variant>

namespace synthizer {

namespace mod_pointer_detail {
/**
 * A length provider based off a static compile-time-known len.
 * */
template <std::size_t LEN> class StaticLengthProvider {
public:
  std::size_t getLength() const { return LEN; }
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
template <typename T, typename LEN_PROVIDER_T> class ModSlice {
public:
  ModSlice(T *_data, std::size_t initial_offset, LEN_PROVIDER_T length_provider);

  T &operator[](std::size_t index) const;
  T &operator*() const;
  T *operator->() const;

  ModSlice<T, LEN_PROVIDER_T> &operator++();
  ModSlice<T, LEN_PROVIDER_T> &operator--();
  ModSlice<T, LEN_PROVIDER_T> operator++(int);
  ModSlice<T, LEN_PROVIDER_T> operator--(int);
  ModSlice<T, LEN_PROVIDER_T> operator+(std::size_t increment) const;
  ModSlice<T, LEN_PROVIDER_T> operator-(std::size_t decrement) const;
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
  LEN_PROVIDER_T length_provider;
};

/**
 * A ModPointer is a variant of pointer-like objects to a slice of memory at some offset, where access to the object may
 * need mod.
 *
 * The idea here is that when reading from a delay line for example, if we know how far ahead we're going to read, we
 * may be able to return a raw pointer.  Likewise for powers of 2, and so on.
 *
 * While the non-pointer variants aren't guaranteed to be anything in particular save that they offer a pointer-like
 * interface, there will always be a T* variant and it will always be used when no modulus is required.  See the below
 * helper function for construction of ModPointers.
 *
 * For now, we assume the caller is going to know the length of the underlying memory at compile time.
 * */
template <typename T, std::size_t LEN>
using ModPointer = std::variant<ModSlice<T, mod_pointer_detail::StaticLengthProvider<LEN>>, T *>;

/**
 * Given a pointer, an offset, and one past the maximum index which may be accessed, return a ModPointer that can handle
 * it.
 * */
template <typename T, std::size_t LEN>
ModPointer<T, LEN> createModPointer(T *data, std::size_t offset, std::size_t len);

template <typename T, typename LEN_PROVIDER_T>
ModSlice<T, LEN_PROVIDER_T>::ModSlice(T *_data, std::size_t initial_offset, LEN_PROVIDER_T _length_provider)
    : data(_data), offset(initial_offset), length_provider(_length_provider) {}

template <typename T, typename LEN_PROVIDER_T>
FLATTENED std::size_t ModSlice<T, LEN_PROVIDER_T>::addIndexRelative(std::size_t increment) const {
  return (this->offset + increment) % this->length_provider.getLength();
}

template <typename T, typename LEN_PROVIDER_T>
FLATTENED std::size_t ModSlice<T, LEN_PROVIDER_T>::subIndexRelative(std::size_t decrement) const {
  assert(decrement <= this->length_provider.getLength());
  return (this->length_provider.getLength() + this->offset - decrement) % this->length_provider.getLength();
}

template <typename T, typename LEN_PROVIDER_T>
FLATTENED T &ModSlice<T, LEN_PROVIDER_T>::operator[](std::size_t index) const {
  auto actual_index = this->addIndexRelative(index);
  return this->data[actual_index];
}

template <typename T, typename LEN_PROVIDER_T> FLATTENED T &ModSlice<T, LEN_PROVIDER_T>::operator*() const {
  return (*this)[0];
}

template <typename T, typename LEN_PROVIDER_T> T *ModSlice<T, LEN_PROVIDER_T>::operator->() const {
  return &((*this)[0]);
}

template <typename T, typename LEN_PROVIDER_T> ModSlice<T, LEN_PROVIDER_T> &ModSlice<T, LEN_PROVIDER_T>::operator++() {
  this->offset = this->addIndexRelative(1);
  return *this;
}

template <typename T, typename LEN_PROVIDER_T> ModSlice<T, LEN_PROVIDER_T> &ModSlice<T, LEN_PROVIDER_T>::operator--() {
  // This is equivalent to going around one minus the len.
  this->offset = this->subIndexRelative(1);
  return *this;
}

template <typename T, typename LEN_PROVIDER_T>
ModSlice<T, LEN_PROVIDER_T> ModSlice<T, LEN_PROVIDER_T>::operator++(int) {
  auto old = *this;
  ++(*this);
  return old;
}

template <typename T, typename LEN_PROVIDER_T>
ModSlice<T, LEN_PROVIDER_T> ModSlice<T, LEN_PROVIDER_T>::operator--(int) {
  auto old = *this;
  --(*this);
  return old;
}

template <typename T, typename LEN_PROVIDER_T>
ModSlice<T, LEN_PROVIDER_T> ModSlice<T, LEN_PROVIDER_T>::operator+(std::size_t increment) const {
  auto copy = *this;
  copy.offset = this->addIndexRelative(increment);
  return copy;
}

template <typename T, typename LEN_PROVIDER_T>
ModSlice<T, LEN_PROVIDER_T> ModSlice<T, LEN_PROVIDER_T>::operator-(std::size_t decrement) const {
  auto copy = *this;
  copy.offset = this->subIndexRelative(decrement);
  return copy;
}

template <typename T, typename LEN_PROVIDER_T>
FLATTENED void ModSlice<T, LEN_PROVIDER_T>::operator+=(std::size_t increment) {
  *this = *this + increment;
}

template <typename T, std::size_t LEN>
FLATTENED ModPointer<T, LEN> createModPointer(T *data, std::size_t offset, std::size_t len) {
  if (offset + len > LEN) {
    return ModSlice<T, mod_pointer_detail::StaticLengthProvider<LEN>>{data, offset,
                                                                      mod_pointer_detail::StaticLengthProvider<LEN>{}};
  } else {
    return data + offset;
  }
}

} // namespace synthizer
