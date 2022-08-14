#pragma once

#include "synthizer/config.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/trylock.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace synthizer {

const std::size_t MAX_BLOCK_BUFFER_CACHE_ENTRIES = 16;

/**
 * The BlockBufferCache returns buffers sized to fit one block of audio data, specifically config::BLOCK_SIZE *
 * config::MAX_CHANNELS.
 *
 * This is used to replace thread_local blocks in lots of places, so that the bound on the number of blocks is
 * effectively the depth of recursion, not how many functions decided to use thread_local.  It also provides safety
 * around cases wherein objects might get called recursively while using thread_local data, such that deeper levels of
 * recursion accidentally override shallower levels.
 *
 * This is done through a non-movable smart pointer which releases the buffer on destruction and coerces to a float *.
 * */

/**
 * The smart pointer for the cache. Coerces to float * as necessary.
 * */
class BlockBufferGuard {
public:
  BlockBufferGuard() = delete;
  BlockBufferGuard(const BlockBufferGuard &other) = delete;
  BlockBufferGuard(const BlockBufferGuard &&other) = delete;
  ~BlockBufferGuard();

  operator float *() { return this->data; }

  std::size_t size() { return config::BLOCK_SIZE * config::MAX_CHANNELS; }

private:
  explicit BlockBufferGuard(float *data) : data(data) {}

  float *data = nullptr;

  friend BlockBufferGuard acquireBlockBuffer(bool should_zero);
};

/**
 * Acquire a block from the cache.
 *
 * May allocate if the cache overflow.
 * */
BlockBufferGuard acquireBlockBuffer(bool should_zero = true);

namespace bbc_detail {

class BlockBufferCache {
public:
  BlockBufferCache() {
    this->count = 3;
    for (std::size_t i = 0; i < this->count; i++) {
      this->entries[i] = (float *)std::calloc(config::BLOCK_SIZE * config::MAX_CHANNELS, sizeof(float));
    }
  }

  /* It's internally a stack. */
  std::array<float *, MAX_BLOCK_BUFFER_CACHE_ENTRIES> entries = {{nullptr}};
  std::size_t count = 0;
};

inline TryLock<BlockBufferCache> block_buffer_cache{};
} // namespace bbc_detail

inline BlockBufferGuard acquireBlockBuffer(bool should_zero) {
  using namespace bbc_detail;

  float *out = nullptr;

  block_buffer_cache.withLock([&](auto *cache) {
    if (cache->count == 0) {
      /* Handle allocation at the end of the function, outside the lock. */
      return;
    }
    cache->count -= 1;
    out = cache->entries[cache->count];
  });

  if (out == nullptr) {
    out = (float *)std::calloc(config::BLOCK_SIZE * config::MAX_CHANNELS, sizeof(float));
  } else if (should_zero) {
    std::fill(out, out + config::BLOCK_SIZE * config::MAX_CHANNELS, 0.0f);
  }

  return BlockBufferGuard(out);
}

inline BlockBufferGuard::~BlockBufferGuard() {
  using namespace bbc_detail;

  block_buffer_cache.withLock([&](auto *cache) {
    if (cache->count == cache->entries.size()) {
      return;
    }
    cache->entries[cache->count] = this->data;
    cache->count++;
    /* Give the cache ownership. */
    this->data = nullptr;
  });
  if (this->data != nullptr) {
    deferredFree(this->data);
  }
}

} // namespace synthizer
