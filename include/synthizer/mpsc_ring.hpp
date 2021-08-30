#pragma once

#include "synthizer/at_scope_exit.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/property_internals.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

namespace synthizer {

enum MPSC_RING_ENTRY_STATE {
  MPSC_RING_E_EMPTY = 0,
  MPSC_RING_E_ALLOCATED_FOR_WRITE = 1,
  MPSC_RING_E_READY = 2,
};

template <typename T> struct MpscRingEntry {
  T value;
  std::atomic<unsigned int> state = MPSC_RING_E_EMPTY;
};

template <typename T, std::size_t capacity> class MpscRing {
  static_assert(capacity != 0);

public:
  /* Write to the ring with a callback. Returns false if the ring is full. */
  template <typename CB> bool write(CB &&callback);

  /* Returns false if the ring is full. */
  bool enqueue(const T &&value);

  template <typename CB> void processAll(CB &&callback);

private:
  std::array<MpscRingEntry<T>, capacity> ring{};
  std::atomic<std::uint64_t> reader_index = 0;
  std::atomic<std::uint64_t> writer_index = 0;
};

template <typename T, std::size_t capacity> template <typename CB> bool MpscRing<T, capacity>::write(CB &&callback) {
  /*
   * To allocate, we load both pointers, find out if the distance is less than the capacity, then try to increment.
   *
   * From an academic perspective this is incorrect, but it would take well over 5000 years of writing 100m elements per
   * second before either index wraps.
   * */
  std::size_t index;
  while (1) {
    auto start = this->reader_index.load(std::memory_order_relaxed);
    auto end = this->writer_index.load(std::memory_order_relaxed);
    auto dist = end - start;
    if (dist >= capacity) {
      /* We'd get "behind" the write pointer. */
      return false;
    }
    /* try to mark end as ours. If we can, increment the index. */
    index = end % capacity;
    unsigned int state = MPSC_RING_E_EMPTY;
    if (this->ring[index].state.compare_exchange_strong(state, MPSC_RING_E_ALLOCATED_FOR_WRITE,
                                                        std::memory_order_acquire)) {
      this->writer_index.fetch_add(1, std::memory_order_relaxed);
      break;
    }
  }

  callback(this->ring[index].value);
  this->ring[index].state.store(MPSC_RING_E_READY, std::memory_order_release);

  return true;
}

template <typename T, std::size_t capacity> bool MpscRing<T, capacity>::enqueue(const T &&value) {
  return this->write([&](T &val) { val = value; });
}

template <typename T, std::size_t capacity>
template <typename CB>
void MpscRing<T, capacity>::processAll(CB &&callback) {
  while (1) {
    auto r = this->reader_index.load(std::memory_order_relaxed);
    auto &cell = this->ring[r % capacity];
    auto state = cell.state.load(std::memory_order_acquire);
    if (state != MPSC_RING_E_READY) {
      return;
    }
    {
      /** We always increment the reader index and set the cell to ready, even on exception.
       * This prevents an infinite loop, should an entry's processing throw repeatedly.
       * */
      auto a = AtScopeExit([&]() {
        cell.state.store(MPSC_RING_E_EMPTY, std::memory_order_release);
        this->reader_index.fetch_add(1, std::memory_order_relaxed);
      });
      callback(cell.value);
    }
  }
}

} // namespace synthizer
