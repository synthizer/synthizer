#include "utils.hpp"

#include <benchmark/benchmark.h>

int main(int argc, char **argv) {
  syz_bench::SynthizerInitGuard init_guard{};

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
