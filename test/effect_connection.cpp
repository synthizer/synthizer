/*
 * Try to break the effects infrastructure by connecting and disconnecting sources to a reverb.
 *
 * Introduced because of issue #50, which proved that this is fragile enough to deserve a test which tries to crash it.
 * In the case of #50, uncovered bugs in the command infrastructure.
 *
* Currently, even though this compiles it doesn't work because headless contexts aren't good enough.  We disable by
 *default, and can revisit this whenever we revisit headless contexts.
 * */
#include "synthizer.h"
#include "synthizer/property_xmacros.hpp"
#include "synthizer_constants.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#define TICK_CTX REQUIRE_FALSE(syz_contextGetBlock(ctx, &tmp_buf[0]))
TEST_CASE("Hammer on effect connections to try to find crashes", "[.]") {
  syz_Handle ctx, source1, source2, source3, reverb1, reverb2;
  std::array<float, 2 * 1024> tmp_buf;
  syz_RouteConfig route_cfg{1.0, 0.1};

  REQUIRE_FALSE(syz_createContextHeadless(&ctx, NULL, NULL));

  REQUIRE_FALSE(syz_createSource3D(&source1, ctx, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, 0.0, NULL, NULL, NULL));
  REQUIRE_FALSE(syz_createSource3D(&source2, ctx, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, 0.0, NULL, NULL, NULL));
  REQUIRE_FALSE(syz_createSource3D(&source3, ctx, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, 0.0, NULL, NULL, NULL));
  REQUIRE_FALSE(syz_createGlobalFdnReverb(&reverb1, ctx, NULL, NULL, NULL));
  REQUIRE_FALSE(syz_createGlobalFdnReverb(&reverb2, ctx, NULL, NULL, NULL));

  TICK_CTX;

  std::vector<syz_Handle> sources{source1, source2, source3};
  std::vector<syz_Handle> reverbs{reverb1, reverb2};

  for (auto s : sources) {
    for (auto r : reverbs) {
      REQUIRE_FALSE(syz_routingConfigRoute(ctx, s, r, &route_cfg));
    }
  }

  TICK_CTX;

  for (auto s : sources) {
    for (auto r : reverbs) {
      REQUIRE_FALSE(syz_routingRemoveRoute(ctx, s, r, 0.01));
    }
  }

  TICK_CTX;

  for (auto r : reverbs) {
    for (auto s : sources) {
      REQUIRE_FALSE(syz_routingConfigRoute(ctx, s, r, &route_cfg));
      REQUIRE_FALSE(syz_routingRemoveRoute(ctx, s, r, 0.05));
      TICK_CTX;
    }
  }

  TICK_CTX;

  for (auto s : sources) {
    REQUIRE_FALSE(syz_handleDecRef(s));
    TICK_CTX;
  }

  for (auto r : reverbs) {
    REQUIRE_FALSE(syz_handleDecRef(r));
    TICK_CTX;
  }

  for (auto s : sources) {
    REQUIRE_FALSE(syz_handleDecRef(s));
  }

  for (auto r : reverbs) {
    REQUIRE_FALSE(syz_handleDecRef(r));
  }

  REQUIRE_FALSE(syz_handleDecRef(ctx));
}
