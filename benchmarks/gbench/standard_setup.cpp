/**
 * This benchmarks the "standard setup": some number of sources sharing a buffer with some number of generators each,
 * and some number of reverbs.
 * */
#include "utils.hpp"

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/config.hpp"
#include "synthizer/random_generator.hpp"

#include <benchmark/benchmark.h>

#include <array>
#include <vector>

void bm_standard_setup(benchmark::State &state, int panner_strategy, unsigned int num_generators,
                       unsigned int num_sources, unsigned int num_reverbs) {
  std::array<float, synthizer::config::BLOCK_SIZE * 2> output_buf;
  syz_bench::HandleSet handles{};
  syz_Handle context;
  // We leave generators and reverbs alone, but we need to touch sources.
  std::vector<syz_Handle> sources;
  syz_Handle buffer;
  std::vector<float> buffer_data{};
  synthizer::RandomGenerator rand_gen;

  // We don't want to give the compiler any hope of optimizing, so make the buffer random data.
  //
  // We also want to do our best to make sure that this is realistic, so make it long enough.
  for (unsigned int i = 0; i < 44100; i++) {
    buffer_data.push_back(rand_gen.generateFloat());
  }

  SYZ_CHECKED(syz_createContextHeadless(&context, NULL, NULL));
  handles.registerHandle(context);

  SYZ_CHECKED(syz_createBufferFromFloatArray(&buffer, synthizer::config::SR, 1, buffer_data.size(), buffer_data.data(),
                                             NULL, NULL));

  for (unsigned int i = 0; i < num_sources; i++) {
    syz_Handle source;

    SYZ_CHECKED(syz_createSource3D(&source, context, panner_strategy, 0.0, 0.0, 0.0, NULL, NULL, NULL));
    handles.registerHandle(source);
    sources.push_back(source);

    for (unsigned int j = 0; j < num_generators; j++) {
      syz_Handle gen;

      SYZ_CHECKED(syz_createBufferGenerator(&gen, context, NULL, NULL, NULL));
      handles.registerHandle(gen);
      // Looping means it's always doing something; while we could just let it not, it'll eventually stop doing work,
      // and that breaks gbench's assumptions.
      SYZ_CHECKED(syz_setI(gen, SYZ_P_LOOPING, 1));
      SYZ_CHECKED(syz_setO(gen, SYZ_P_BUFFER, buffer));
      SYZ_CHECKED(syz_sourceAddGenerator(source, gen));
    }
  }

  for (unsigned int i = 0; i < num_reverbs; i++) {
    syz_Handle reverb;

    SYZ_CHECKED(syz_createGlobalFdnReverb(&reverb, context, NULL, NULL, NULL));

    for (auto s : sources) {
      syz_RouteConfig cfg;
      syz_initRouteConfig(&cfg);

      SYZ_CHECKED(syz_routingConfigRoute(context, s, reverb, &cfg));
    }
  }

  for (auto _ : state) {
    SYZ_CHECKED(syz_contextGetBlock(context, &output_buf[0]));
  }
}

// pattern is panner strategy, generators per source, sources, reverbs.

// Let's start by making sure that our really small cases don't do weird things.
BENCHMARK_CAPTURE(bm_standard_setup, "sg0s0r0", SYZ_PANNER_STRATEGY_STEREO, 0, 0, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "hg0s0r0", SYZ_PANNER_STRATEGY_HRTF, 0, 0, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r0", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "hg1s1r0", SYZ_PANNER_STRATEGY_HRTF, 1, 1, 0);

// Let's now just climb the sources up the HRTF ladder.
BENCHMARK_CAPTURE(bm_standard_setup, "hg1s10r0", SYZ_PANNER_STRATEGY_HRTF, 1, 10, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "hg1s30r0", SYZ_PANNER_STRATEGY_HRTF, 1, 30, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "hg1s100r0", SYZ_PANNER_STRATEGY_HRTF, 1, 100, 0);

// And same thing, but this time for stereo.
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s10r0", SYZ_PANNER_STRATEGY_STEREO, 1, 10, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s30r0", SYZ_PANNER_STRATEGY_STEREO, 1, 30, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s100r0", SYZ_PANNER_STRATEGY_STEREO, 1, 100, 0);

// Let's see about using a single source to benchmark adding a bunch of generators.
BENCHMARK_CAPTURE(bm_standard_setup, "sg2s1r0", SYZ_PANNER_STRATEGY_STEREO, 2, 1, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "sg5s1r0", SYZ_PANNER_STRATEGY_STEREO, 5, 1, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "sg30s1r0", SYZ_PANNER_STRATEGY_STEREO, 30, 1, 0);
BENCHMARK_CAPTURE(bm_standard_setup, "sg100s1r0", SYZ_PANNER_STRATEGY_STEREO, 100, 1, 0);

// Now climb the reverb ladder.
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r1", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 1);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r2", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 2);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r3", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 3);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r4", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 4);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r10", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 10);
BENCHMARK_CAPTURE(bm_standard_setup, "sg1s1r100", SYZ_PANNER_STRATEGY_STEREO, 1, 1, 100);

// These match roughly what we might expect out of a shooter (namely System Fault, which was having performance problems
// at time of writing).
BENCHMARK_CAPTURE(bm_standard_setup, "hg1s30r2", SYZ_PANNER_STRATEGY_HRTF, 1, 30, 2);
BENCHMARK_CAPTURE(bm_standard_setup, "hg1s100r2", SYZ_PANNER_STRATEGY_HRTF, 1, 100, 2);
