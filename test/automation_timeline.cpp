#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/automation_timeline.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace synthizer;

bool floatCmp(double a, double b) {
	return std::abs(a - b) < 0.000001;
}

int main() {
	std::vector<syz_AutomationPoint> points{{
		{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.0, { 1.0 }},
		{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.01, { 0.5 }},
		{ SYZ_INTERPOLATION_TYPE_NONE, 0.02, { 0.1 }},
		{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.05, { 0.0 }},
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

	ExposedAutomationTimeline et(points.size(), &points[0]);
	auto timeline = et.buildTimeline();
	for (auto exp: expected) {
		timeline->tick();
		auto v = timeline->getValue();
		if (v) {
			auto x = *v;
			if (floatCmp(x, exp) == false) {
				printf("Expected %f but got %f at time %f\n", exp, x, timeline->getTimeInSeconds());
				return 1;
			}
		} else {
			printf("Timeline ended early at time %f\n", timeline->getTimeInSeconds());
			return 1;
		}
	}

	if (timeline->isFinished() == false) {
		printf("Timeline should be finished, but isn't\n");
		return 1;
	}

	timeline->tick();
	if (timeline->getValue()) {
		printf("Finished timelines should no longer return values\n");
		return 1;
	}

	printf("Success\n");
	return 0;
}
