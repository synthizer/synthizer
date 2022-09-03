#include "synthizer/math.hpp"

#include <catch2/catch_all.hpp>

using namespace synthizer;

TEST_CASE("floorByPowerOfTwo") {
  REQUIRE(floorByPowerOfTwo(0, 4) == 0);
  REQUIRE(floorByPowerOfTwo(1, 4) == 0);
  REQUIRE(floorByPowerOfTwo(2, 4) == 0);
  REQUIRE(floorByPowerOfTwo(3, 4) == 0);
  REQUIRE(floorByPowerOfTwo(4, 4) == 4);
  REQUIRE(floorByPowerOfTwo(1000, 4) == 1000);
}

TEST_CASE("ceilByPowerOfTwo") {
  REQUIRE(ceilByPowerOfTwo(0, 4) == 0);
  REQUIRE(ceilByPowerOfTwo(1, 4) == 4);
  REQUIRE(ceilByPowerOfTwo(2, 4) == 4);
  REQUIRE(ceilByPowerOfTwo(3, 4) == 4);
  REQUIRE(ceilByPowerOfTwo(4, 4) == 4);
  REQUIRE(ceilByPowerOfTwo(5, 4) == 8);
}
