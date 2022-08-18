#include "synthizer/random_generator.hpp"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define FCHECK(x)                                                                                                      \
  REQUIRE(x >= -1.0f);                                                                                                 \
  REQUIRE(x <= 1.0f)

TEST_CASE("RandomGenerator: Random floats are always in range") {
  synthizer::RandomGenerator gen{};

  SECTION("Generating flaots one at a time") {
    for (unsigned int i = 0; i < 100000; i++) {
      float v = gen.generateFloat();
      FCHECK(v);
    }
  }

  SECTION("Generating in batches of 4") {
    for (unsigned int i = 0; i < 100000; i++) {
      float f1, f2, f3, f4;
      gen.generateFloat4(f1, f2, f3, f4);
      FCHECK(f1);
      FCHECK(f2);
      FCHECK(f3);
      FCHECK(f4);
    }
  }
}
