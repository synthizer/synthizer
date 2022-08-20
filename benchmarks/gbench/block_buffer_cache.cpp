#include "synthizer/block_buffer_cache.hpp"

#include <benchmark/benchmark.h>

void bm_block_buffer_cache(benchmark::State &state) {
  for (auto _ : state) {
    // Don't zero, so that we only measure the contribution of the cache.
    benchmark::DoNotOptimize(synthizer::acquireBlockBuffer(false));
  }
}

BENCHMARK(bm_block_buffer_cache);
