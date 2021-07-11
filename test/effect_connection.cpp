/*
 * Try to break the effects infrastructure by connecting and disconnecting sources to a reverb.
 * 
 * Introduced because of issue #50, which proved that this is fragile enough to deserve a test
 * which tries to crash it.  In the case of #50, uncovered bugs in the command infrastructure.
 * 
 * At this time, this isn't part of the unit tests and needs to be run manually: headless contexts don't introduce threads, and we don't have non-headless contexts that can run on CI without audio devices.
 * */
#include "synthizer.h"
#include "synthizer_constants.h"
#include "synthizer/property_xmacros.hpp"

#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

/* We just let this test leak, since it's not an example. */
#define CHECKED(x) if ((x) != 0) { printf("Failure: " #x); return 1; }
//#define TICK_CTX() CHECKED(syz_contextGetBlock(ctx, &tmp_buf[0]))
#define TICK_CTX()


int main() {
	syz_Handle ctx, source1, source2, source3, reverb1, reverb2;
	std::array<float, 2 * 1024> tmp_buf;
	syz_RouteConfig route_cfg{ 1.0, 0.1 };

	CHECKED(syz_initialize());

	//CHECKED(syz_createContextHeadless(&ctx));
	CHECKED(syz_createContext(&ctx, NULL, NULL));

	CHECKED(syz_createSource3D(&source1, ctx, NULL, NULL));
	CHECKED(syz_createSource3D(&source2, ctx, NULL, NULL));
	CHECKED(syz_createSource3D(&source3, ctx, NULL, NULL));
	CHECKED(syz_createGlobalFdnReverb(&reverb1, ctx, NULL, NULL));
	CHECKED(syz_createGlobalFdnReverb(&reverb2, ctx, NULL, NULL));

	TICK_CTX();

	std::vector<syz_Handle> sources{source1, source2, source3};
	std::vector<syz_Handle> reverbs{reverb1, reverb2};

	for (auto s: sources) {
		for (auto r: reverbs) {
			CHECKED(syz_routingConfigRoute(ctx, s, r, &route_cfg));
		}
	}

	TICK_CTX();

	for (auto s: sources) {
		for (auto r: reverbs) {
			CHECKED(syz_routingRemoveRoute(ctx, s, r, 0.01));
		}
	}

	TICK_CTX();

	for (auto r: reverbs) {
		for (auto s: sources) {
			CHECKED(syz_routingConfigRoute(ctx, s, r, &route_cfg));
			CHECKED(syz_routingRemoveRoute(ctx, s, r, 0.05));
			TICK_CTX();
		}
	}

	TICK_CTX();

	for(auto s: sources) {
		CHECKED(syz_handleDecRef(s));
		TICK_CTX();
	}

	for (auto r: reverbs) {
		CHECKED(syz_handleDecRef(r));
		TICK_CTX();
	}

	CHECKED(syz_shutdown());

	/**
	 * Valuable when running from the command line since if this doesn't rpint, the test crashed in the middle.
	 * */
	printf("Done\n");

	return 0;
}
