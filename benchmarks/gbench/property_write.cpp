#include "synthizer.h"
#include "synthizer_constants.h"

#include "utils.hpp"

#include <benchmark/benchmark.h>

void bm_property_write(benchmark::State &state) {
  syz_bench::HandleSet handles{};
  syz_Handle context, source;

  SYZ_CHECKED(syz_createContext(&context, NULL, NULL));
  handles.registerHandle(context);

  SYZ_CHECKED(syz_createSource3D(&source, context, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, 0.0, NULL, NULL, NULL));
  handles.registerHandle(source);

  for (auto _ : state) {
    SYZ_CHECKED(syz_setD(source, SYZ_P_GAIN, 1.0));
  }
}

BENCHMARK(bm_property_write);
