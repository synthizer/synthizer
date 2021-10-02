#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/config.hpp"
#include "synthizer/property_automation_timeline.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace synthizer;

bool floatCmp(double a, double b) { return std::abs(a - b) < 0.000001; }

struct Point {
  int type;
  double time;
  double value;
};

int main() {
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
    if (v) {
      auto x = *v;
      if (floatCmp(x[0], exp) == false) {
        printf("Expected %f but got %f at time %f\n", exp, x[0], time);
        return 1;
      }
    } else {
      printf("Timeline ended early at time %f\n", time);
      return 1;
    }
  }

  printf("Success\n");
  return 0;
}
