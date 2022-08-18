#include "synthizer/delay_line.hpp"

#include <catch2/catch_all.hpp>

#include <cstdio>

TEST_CASE("Basic, non-optimized delay lines") {
  auto dl = new synthizer::DelayLine<int, 32>();
  for (int i = 0; i < 10000; i++) {
    auto read = dl->read(5);
    unsigned int expected = i < 5 ? 0 : i - 5;
    REQUIRE(read == expected);

    dl->write(i);
    dl->advance();
  }
  delete dl;
}
