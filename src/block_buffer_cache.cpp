#include "synthizer/block_buffer_cache.hpp"

#include "synthizer/config.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/trylock.hpp"

#include <array>
#include <cstdlib>

namespace synthizer {

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

static TryLock<BlockBufferCache> block_buffer_cache{};

BlockBufferGuard acquireBlockBuffer(bool should_zero) {
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

BlockBufferGuard::~BlockBufferGuard() {
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
