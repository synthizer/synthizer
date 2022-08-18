#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/config.hpp"
#include "synthizer/property_automation_timeline.hpp"

#include <catch2/catch_all.hpp>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace synthizer;

void floatCmp(double a, double b) { REQUIRE(a == Catch::Approx(b)); }

struct Point {
  int type;
  double time;
  double value;
};

TEST_CASE("Test storing a curve in PropertyAutomationTimeline") {
  double tick_delta = config::BLOCK_SIZE / (double)config::SR;
  double time = 0.0;

  std::vector<Point> points{{
      {SYZ_INTERPOLATION_TYPE_LINEAR, 0.0, 1.0},
      {SYZ_INTERPOLATION_TYPE_LINEAR, 0.01, 0.5},
      {SYZ_INTERPOLATION_TYPE_NONE, 0.02, 0.1},
      {SYZ_INTERPOLATION_TYPE_LINEAR, 0.05, 0.0},
  }};

  std::vector<double> expected{{
      1.000000,
      0.709751,
      0.500000,
      0.500000,
      0.089267,
      0.069917,
      0.050567,
      0.031217,
      0.011867,
      0.000000,
  }};

  PropertyAutomationTimeline<6> timeline{};
  for (auto i : points) {
    struct syz_AutomationPoint p {};
    p.interpolation_type = i.type;
    p.values[0] = i.value;
    timeline.addPoint(PropertyAutomationPoint<6>(i.time, &p));
  }

  for (auto exp : expected) {
    timeline.tick(time);
    time += tick_delta;
    auto v = timeline.getValue();
    REQUIRE(v);
    auto x = *v;
    floatCmp(x[0], exp);
  }
}

class IdentityPoint {
public:
  IdentityPoint(double t) : value(t) {}
  double getTime() { return this->value; }
  double value;
};

/**
 * Test a timeline which "runs ahead". Exercises logic which gets rid of unneeded points.
 * */
TEST_CASE("Test point removal on PropertyAutomationTimeline") {
  auto tl = GenericTimeline<IdentityPoint, 10>();
  unsigned int i = 0, last_added = 0;

  // Add an initial 10 points.
  for (i = 0; i < 10; i++) {
    tl.addItem(IdentityPoint((double)i));
  }

  // We want the 10th point for convenience; always having one in the future for the next loop avoids a weird special
  // case where the first iteration returns the wrong value w.r.t. what we check because there is no point "after" the
  // start time.
  last_added = i;
  tl.addItem(IdentityPoint(last_added));

  for (; i < 10000; i++) {
    // Drive the timeline to time i-0.5, which gives us the next point as i.
    tl.tick((double)i - 0.5);
    // We know there are at least 10 points; check them all.
    for (unsigned int d = 0; d < 10; d++) {
      double p = (double)i - d;
      auto val = tl.getItem(-d);
      REQUIRE(val);
      REQUIRE((*val)->value == (double)p);
    }

    // So the timeline always grows, add two items.
    for (unsigned int _unused = 0; _unused < 2; _unused++) {
      last_added += 1;
      tl.addItem(IdentityPoint((double)last_added));
    }
  }
}
