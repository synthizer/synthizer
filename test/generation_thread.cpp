/**
 * Try to detect races in GenerationThread by writing increasing floats and seeing if
 * it falls over (and if so, when).
 * */
#include "synthizer/generation_thread.hpp"
#include "synthizer/config.hpp"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

using namespace synthizer;

TEST_CASE("Test the generation thread") {
  std::array<float, config::BLOCK_SIZE> tmp_buf;
  std::vector<int> ints;
  GenerationThread<int *> gt{5};
  std::size_t read_so_far = 0;
  const std::size_t iterations = 10000;
  int gen_int = 0;

  ints.resize(10);
  for (auto &i : ints) {
    gt.send(&i);
  }

  gt.start([&](int **item) {
    **item = gen_int;
    gen_int++;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  });

  for (int i = 0; i < iterations; i++) {
    int *got;
    if (gt.receive(&got) == false) {
      continue;
    }
    REQUIRE(*got == i);
    gt.send(got);
  }
}
