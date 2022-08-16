#include "synthizer/panning/hrtf_panner.hpp"
#include "synthizer/random_generator.hpp"

#include <benchmark/benchmark.h>

#include <array>

static void bm_hrtf_panner(benchmark::State &state) {
  // We flip the sign of azimuth to force it to crossfade.
  double azimuth = 45.0;

  std::array<float, synthizer::config::BLOCK_SIZE * 2> dest_buf{0.0};

  auto rgen = synthizer::RandomGenerator{};

  synthizer::HrtfPanner panner{};

  for (auto s : state) {
    panner.setPanningAngles(azimuth, 0.0);
    azimuth *= -1.0;

    float *dest = panner.getInputBuffer();

    for (unsigned int i = 0; i < synthizer::config::BLOCK_SIZE; i++) {
      dest[i] = rgen.generateFloat();
    }

    panner.run(&dest_buf[0]);
    // Now to prevent the compiler optimizing, sum dest_buf and then block box it.
    float sum = 0.0f;
    for (auto i : dest_buf)
      sum += i;
    benchmark::DoNotOptimize(sum);
  }
}

BENCHMARK(bm_hrtf_panner);
