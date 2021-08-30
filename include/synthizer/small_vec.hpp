#pragma once

#include <array>
#include <cstdint>
#include <utility>

namespace synthizer {
/**
 * A simple, inline-allocated vector.
 *
 * For simplicity, will currently default construct all unused objects--that is, if MAX_CAPACITY == 16, you get
 * 16 default constructor calls even though no items are used yet.  It's possible to change this, but for the sake of
 * implementation simplicity we opt not to until performance becomes a problem.
 * */
template <typename T, std::size_t MAX_CAPACITY> class SmallVec {
public:
  /**
   * Returns whether the element was pushed.
   * */
  bool pushBack(const T &element) {
    T *dest = this->preparePush();
    if (dest == nullptr) {
      return false;
    }
    *dest = element;
    return true;
  }

  /**
   * Returns whether the element was pushed.
   * */
  bool pushBack(T &&element) {
    T *dest = this->preparePush();
    if (dest == nullptr) {
      return false;
    }
    *dest = std::move(element);
    return true;
  }

  auto begin() { return this->storage.begin(); }

  auto end() { return this->storage.begin() + this->length; }

  std::size_t size() { return this->length; }

  bool empty() { return this->length == 0; }

  T &operator[](std::size_t index) {
    assert(index < this->size());
    return this->storage[index];
  }

private:
  T *preparePush() {
    if (this->length == MAX_CAPACITY) {
      return nullptr;
    }
    T *out = &this->storage[this->length];
    this->length++;
    return out;
  }

  std::array<T, MAX_CAPACITY> storage;
  std::size_t length = 0;
};

} // namespace synthizer
