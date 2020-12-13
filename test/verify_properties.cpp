/*
 * verifies some key invariants about properties:
 * - Int/double properties can be set to their minimum and maximum.
 * - Int/double properties can't be set outside their range.
 * - double3/double6 properties can be set.
 * - Object properties can be read.
 * 
 * We don't do more with object properties because it isn't possible to generically set objects to them, and we
 * can't check every object because they're also "running" when we tick the context (e.g. BufferGenerator moves position).
 * 
 * Note that the below uses a lot of preprocessor black magic to read the property list from
 * property_xmacros.hpp.
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

const auto double_min = std::numeric_limits<double>::lowest();
const auto double_max = std::numeric_limits<double>::max();
#define P_DOUBLE_MIN double_min
#define P_DOUBLE_MAX	 double_max
void printLastError() {
	const char *msg = syz_getLastErrorMessage();
	long long code = syz_getLastErrorCode();
	std::printf("%s (%llu)\n", msg, code);
}

static std::array<float, 2 * 1024> tmp_buf;

#define ERR_MSG(msg) std::printf("Error %s.%s: " msg ":", objtype, propname); printLastError(); std::exit(1)
#define TICK_CTX if (syz_contextGetBlock(ctx, &tmp_buf[0]) != 0) {ERR_MSG("Unable to tick context"); }

void verifyInt(syz_Handle ctx, syz_Handle handle, int property, int min, int max, const char *objtype, const char *propname) {
	printf("Verifying %s.%s\n", objtype, propname);

	int val;
	if (syz_setI(handle, property, min) != 0) {
		ERR_MSG("Unable to set to minimum");
	}
	TICK_CTX;
	if (syz_getI(&val, handle, property) != 0) {
		ERR_MSG("Unable to read value");
	}
	if (val != min) {
		ERR_MSG("Read value isn't the minimum");
	}
	if (syz_setI(handle, property, max) != 0) {
		ERR_MSG("Unable to set to maximum");
	}
	TICK_CTX;
	if (syz_getI(&val, handle, property) != 0) {
		ERR_MSG("Unable to read property");
	}
	if (val != max) {
		ERR_MSG("Value != max");
	}
	if (min > INT_MIN && syz_setI(handle, property, min-1) == 0) {
		ERR_MSG("SHouldn't be able to set below minimum");
	}
	if (max < INT_MAX && syz_setI(handle, property, max+1) == 0) {
		ERR_MSG("Was able to set above maximum");
	}
}

void verifyDouble(syz_Handle ctx, syz_Handle handle, int property, double min, double max, const char *objtype, const char *propname) {
	printf("Verifying %s.%s\n", objtype, propname);

	double val;
	if (syz_setD(handle, property, min) != 0) {
		ERR_MSG("Unable to set to minimum");
	}
	TICK_CTX;
	if (syz_getD(&val, handle, property) != 0) {
		ERR_MSG("Unable to read value");
	}
	if (val != min) {
		ERR_MSG("Read value isn't the minimum");
	}
	if (syz_setD(handle, property, max) != 0) {
		ERR_MSG("Unable to set to maximum");
	}
	TICK_CTX;
	if (syz_getD(&val, handle, property) != 0) {
		ERR_MSG("Unable to read property");
	}
	if (val != max) {
		ERR_MSG("Value != max");
	}
	if (isinf(min) != true  && syz_setD(handle, property, -INFINITY) == 0) {
		ERR_MSG("SHouldn't be able to set below minimum");
	}
	if (isinf(max) != true && syz_setI(handle, property, INFINITY) == 0) {
		ERR_MSG("Was able to set above maximum");
	}
}

void verifyDouble3(syz_Handle ctx, syz_Handle handle, int property, const char *objtype, const char *propname) {
	printf("Verifying %s.%s\n", objtype, propname);

	double x, y, z;

	if (syz_setD3(handle, property, 2.0, 3.0, 4.0) != 0) {
		ERR_MSG("Unable to set Double3 property");
	}
	TICK_CTX;
	if (syz_getD3(&x, &y, &z, handle, property) != 0) {
		ERR_MSG("Unable to read property");
	}
	if (x != 2.0 || y != 3.0 || z != 4.0) {
		ERR_MSG("Property value mismatch");
	}
}

void verifyDouble6(syz_Handle ctx, syz_Handle handle, int property, const char *objtype, const char *propname) {
	printf("Verifying %s.%s\n", objtype, propname);

	double x, y, z, a, b, c;

	if (syz_setD6(handle, property, -1.0, 0.0, 0.0, 0.0, -1.0, 0.0) != 0) {
		ERR_MSG("Unable to set Double3 property");
	}
	TICK_CTX;
	if (syz_getD6(&x, &y, &z, &a, &b, &c, handle, property) != 0) {
		ERR_MSG("Unable to read property");
	}
	if (x != -1.0 || y != 0.0 || z != 0.0 || a != 0.0 || b != -1.0 || c != 0.0) {
		printf("%f %f %f %f %f %f\n", x, y, z, a, b, c);
		ERR_MSG("Property value mismatch");
	}
}

void verifyObj(syz_Handle ctx, syz_Handle handle, int property, const char *objtype, const char *propname) {
	printf("Verifying %s.%s\n", objtype, propname);
	syz_Handle val;

	if (syz_getO(&val, handle, property) != 0) {
		ERR_MSG("Couldn't read object property");
	}
}

#define INT_P(E, N, IGNORED, MIN, MAX) verifyInt(ctx, handle, E, MIN, MAX, objtype, #N);
/* Need to be able to turn this oiff for BufferGenerator. */
#define DOUBLE_P_IMPL(E, N, IGNORED, MIN, MAX) verifyDouble(ctx, handle, E, MIN, MAX, objtype, #N);
#define DOUBLE_P(...) DOUBLE_P_IMPL(__VA_ARGS__)
#define OBJECT_P(E, N, IGNORED, ...) verifyObj(ctx, handle, E, objtype, #N);
#define DOUBLE3_P(E, N, IGNORED) verifyDouble3(ctx, handle, E, objtype, #N);
#define DOUBLE6_P(E, N, IGNORED) verifyDouble6(ctx, handle, E, objtype, #N);

int main() {
		syz_Handle ctx, handle;
		const char * objtype;

	syz_initialize();

	if (syz_createContextHeadless(&ctx) != 0) {
		printf("Unable to create context: ");
		printLastError();
		return 1;
	}
	handle = ctx;

	objtype = "Context";
	CONTEXT_PROPERTIES;

	objtype = "BufferGenerator";
	if (syz_createBufferGenerator(&handle, ctx) != 0) {
		printf("Couldn't create BufferGenerator");
		printLastError();
		return 1;
	}
	#undef DOUBLE_P
	#define DOUBLE_P(...)
	BUFFER_GENERATOR_PROPERTIES;
	#undef DOUBLE_P
	#define DOUBLE_P(...) DOUBLE_P_IMPL(__VA_ARGS__)

	objtype = "PannedSource";
	if (syz_createPannedSource(&handle, ctx) != 0) {
		printf("Couldn't create PannedSource ");
		printLastError();
		return 1;
	}
	PANNED_SOURCE_PROPERTIES;

	objtype = "Source3D";
	if (syz_createSource3D(&handle, ctx) != 0) {
		printf("Couldn't create Source3D ");
		printLastError();
		return 1;
	}

	objtype = "FdnReverb";
	if (syz_createGlobalFdnReverb(&handle, ctx) != 0) {
		printf("Couldn't create GlobalFdnReverb ");
		printLastError();
		return 1;
	}

	syz_shutdown();
	return 0;
}
