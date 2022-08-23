#include <boost/predef.h>

#if BOOST_HW_SIMD_X86 >= BOOST_HW_SIMD_X86_SSE2_VERSION
#include "synthizer/panning/hrtf_panner.hpp"

#include <catch2/catch_all.hpp>

#include <emmintrin.h>

using namespace synthizer::hrtf_panner_detail;

TEST_CASE("SSE2 horizontal sum") {
  REQUIRE(horizontalSum(_mm_set_ps(4.0f, 3.0f, 2.0f, 1.0f)) == 1.0f + 2.0f + 3.0f + 4.0f);
}

#endif