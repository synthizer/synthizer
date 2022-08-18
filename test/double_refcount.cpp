/**
 * Test that two calls to syz_incRef followed by 2 calls to syz_decRef don't
 * crash etc.
 * */
#include "synthizer.h"

#include <catch2/catch_all.hpp>

TEST_CASE("Check that double increments/decrements of reference counts work") {
  syz_Handle h;

  REQUIRE_FALSE(syz_createContext(&h, NULL, NULL));
  REQUIRE_FALSE(syz_handleIncRef(h));
  REQUIRE_FALSE(syz_handleDecRef(h));
  REQUIRE_FALSE(syz_handleDecRef(h));
}
