#pragma once

#include "synthizer/at_scope_exit.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/property_internals.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace synthizer {

enum MPSC_RING_ENTRY_STATE {
  MPSC_RING_EMPTY = 0,
  MPSC_RING_ENQUEUED = 1,

  // If an exception is thrown while enqueueing, we reach this state and the consumer will skip over it.
  MPSC_RING_CORRUPT = 2,
};

template <typename T> struct MpscRingEntry {
  T value;
  std::atomic<unsigned int> state = MPSC_RING_EMPTY;
};

template <typename T, std::size_t capacity> class MpscRing {
  static_assert(capacity != 0);

public:
  /*
   * Write to the ring with a callback.
   *
   * If the callback throws, the ring instead sets the record to be skipped by the consumer and otherwise leaves the
   * memory untouched.  Though in general callbacks should avoid throwing exceptions, they can do so as long as they can
   * cope with seeing potentially partially initialized memory next time.
   *
   * Returns false if the ring is currently full.
   * */
  template <typename CB> bool write(CB &&callback);

  /**
   * Enqueue an object by moving it into plaec.
   *
   * If the move constructor throws, then the destination memory for the cell will be left in an indeterminate state.
   * Noexcept move constructors are best but, failing that, it's fine if they can handle random bytes in the destination
   * memory.
   * */
  bool enqueue(const T &&value);

  template <typename CB> void processAll(CB &&callback);

private:
  std::array<MpscRingEntry<T>, capacity> ring{};
  std::atomic<std::uint64_t> reader_index = 0;
  std::atomic<std::uint64_t> writer_index = 0;
};

template <typename T, std::size_t capacity> template <typename CB> bool MpscRing<T, capacity>::write(CB &&callback) {
  std::uint64_t producer;

  while (1) {
    // If the distance between the write and read pointer is such that the producer would wrap over top of the consumer
    // we don't continue; this can happen if a bunch of producers enqueue without the consumer having a chance to run
    // and will start overwriting elements.
    //
    // We use the fact that we're uint64_t to simplify things: we don't have to worry about wrapping.
    std::uint64_t consumer = this->reader_index.load(std::memory_order_relaxed);
    producer = this->writer_index.load(std::memory_order_relaxed);

    assert(producer >= consumer);
    // If the ring is capacity 5 then the max index is 4, 4-0 = 4. It's not CAPACITY, it's CAPACITY - 1.
    if (producer - consumer >= capacity - 1) {
      // It's full.
      return false;
    }

    // If we can win the CAS, break out of this loop and proceed to run the callback. Otherwise, spin.
    //
    // Note that since this is C++ and not C, we must use memory_order_acquire: it is possible that the callback needs
    // to examine previous state in order for copy constructors to work.
    if (this->writer_index.compare_exchange_strong(producer, producer + 1, std::memory_order_acquire,
                                                   std::memory_order_relaxed) == true) {
      break;
    }
  }

  std::uint64_t index = producer % capacity;
  assert(this->ring[index].state.load(std::memory_order_relaxed) == MPSC_RING_EMPTY);

  // The callback either succeeds, which is a successful enqueue, or fails, which means we mark it as corrupt.
  try {
    callback(this->ring[index].value);
    this->ring[index].state.store(MPSC_RING_ENQUEUED, std::memory_order_release);
  } catch (...) {
    this->ring[index].state.store(MPSC_RING_CORRUPT, std::memory_order_release);
    throw;
  }

  return true;
}

template <typename T, std::size_t capacity> bool MpscRing<T, capacity>::enqueue(const T &&value) {
  return this->write([&](T &val) { val = std::move(value); });
}

template <typename T, std::size_t capacity>
template <typename CB>
void MpscRing<T, capacity>::processAll(CB &&callback) {
  while (1) {
    auto r = this->reader_index.load(std::memory_order_relaxed);
    auto &cell = this->ring[r % capacity];
    auto state = cell.state.load(std::memory_order_acquire);
    if (state == MPSC_RING_CORRUPT) {
      continue;
    }

    if (state != MPSC_RING_ENQUEUED) {
      return;
    }

    {
      /** We always increment the reader index and set the cell to ready, even on exception.
       * This prevents an infinite loop, should an entry's processing throw repeatedly.
       * */
      auto a = AtScopeExit([&]() {
        cell.state.store(MPSC_RING_EMPTY, std::memory_order_release);
        this->reader_index.fetch_add(1, std::memory_order_relaxed);
      });
      callback(cell.value);
    }
  }
}

} // namespace synthizer
