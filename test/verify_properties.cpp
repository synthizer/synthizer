/*
 * verifies some key invariants about properties:
 * - Int/double properties can be set to their minimum and maximum.
 * - Int/double properties can't be set outside their range.
 * - double3/double6 properties can be set.
 * - Object properties can be read.
 *
 * We don't do more with object properties because it isn't possible to generically set objects to them, and we
 * can't check every object because they're also "running" when we tick the context (e.g. BufferGenerator moves
 * position).
 *
 * Note that the below uses a lot of preprocessor black magic to read the property list from
 * property_xmacros.hpp.
 * */
#include "synthizer.h"
#include "synthizer/property_xmacros.hpp"
#include "synthizer_constants.h"

#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

const auto double_min = std::numeric_limits<double>::lowest();
const auto double_max = std::numeric_limits<double>::max();
#define P_DOUBLE_MIN double_min
#define P_DOUBLE_MAX double_max
void printLastError() {
  const char *msg = syz_getLastErrorMessage();
  long long code = syz_getLastErrorCode();
  std::printf("%s (%llu)\n", msg, code);
}

static std::array<float, 2 * 1024> tmp_buf;

#define ERR_MSG(msg)                                                                                                   \
  std::printf("Error %s.%s: " msg ":", objtype, propname);                                                             \
  printLastError();                                                                                                    \
  std::exit(1)
#define TICK_CTX                                                                                                       \
  if (syz_contextGetBlock(ctx, &tmp_buf[0]) != 0) {                                                                    \
    ERR_MSG("Unable to tick context");                                                                                 \
  }

void verifyInt(syz_Handle ctx, syz_Handle handle, int property, int min, int max, int default_value,
               const char *objtype, const char *propname) {
  printf("Verifying %s.%s\n", objtype, propname);

  int val;
  if (syz_getI(&val, handle, property) != 0) {
    ERR_MSG("Unable to read default");
  }
  if (val != default_value) {
    ERR_MSG("Initial value did not match default");
  }
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
  if (min > INT_MIN && syz_setI(handle, property, min - 1) == 0) {
    ERR_MSG("Shouldn't be able to set below minimum");
  }
  if (max < INT_MAX && syz_setI(handle, property, max + 1) == 0) {
    ERR_MSG("Was able to set above maximum");
  }
}

void verifyDouble(syz_Handle ctx, syz_Handle handle, int property, double min, double max, double default_value,
                  const char *objtype, const char *propname) {
  printf("Verifying %s.%s\n", objtype, propname);

  double val;
  if (syz_getD(&val, handle, property) != 0) {
    ERR_MSG("Unable to read default");
  }
  if (val != default_value) {
    ERR_MSG("Initial value did not match default");
  }
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
  if (std::isinf(min) != true && syz_setD(handle, property, -INFINITY) == 0) {
    ERR_MSG("Shouldn't be able to set below minimum");
  }
  if (std::isinf(max) != true && syz_setI(handle, property, INFINITY) == 0) {
    ERR_MSG("Was able to set above maximum");
  }
}

void verifyDouble3(syz_Handle ctx, syz_Handle handle, int property, double dx, double dy, double dz,
                   const char *objtype, const char *propname) {
  printf("Verifying %s.%s\n", objtype, propname);

  double x, y, z;

  if (syz_getD3(&x, &y, &z, handle, property) != 0) {
    ERR_MSG("Unable to read default");
  }
  if (x != dx || y != dy || z != dz) {
    ERR_MSG("Initial value did not match default");
  }
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

void verifyDouble6(syz_Handle ctx, syz_Handle handle, int property, double dx, double dy, double dz, double da,
                   double db, double dc, const char *objtype, const char *propname) {
  printf("Verifying %s.%s\n", objtype, propname);

  double x, y, z, a, b, c;

  if (syz_getD6(&x, &y, &z, &a, &b, &c, handle, property) != 0) {
    ERR_MSG("Unable to read default");
  }
  if (x != dx || y != dy || z != dz || a != da || b != db || c != dc) {
    ERR_MSG("Initial value did not match default");
  }
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

#define INT_P(E, N, IGNORED, MIN, MAX, DV) verifyInt(ctx, handle, E, MIN, MAX, DV, objtype, #N);
/* Need to be able to turn this off for BufferGenerator. */
#define DOUBLE_P_IMPL(E, N, IGNORED, MIN, MAX, DV) verifyDouble(ctx, handle, E, MIN, MAX, DV, objtype, #N);
#define DOUBLE_P(...) DOUBLE_P_IMPL(__VA_ARGS__)
/* We cant currently test object without dedicated paths. Leave it out for now. */
#define OBJECT_P(...)
#define DOUBLE3_P(E, N, IGNORED, DV1, DV2, DV3) verifyDouble3(ctx, handle, E, DV1, DV2, DV3, objtype, #N);
#define DOUBLE6_P(E, N, IGNORED, DV1, DV2, DV3, DV4, DV5, DV6)                                                         \
  verifyDouble6(ctx, handle, E, DV1, DV2, DV3, DV4, DV5, DV6, objtype, #N);

int main() {
  syz_Handle ctx, source, handle;
  const char *objtype;

  syz_initialize();

  if (syz_createContextHeadless(&ctx, NULL, NULL) != 0) {
    printf("Unable to create context: ");
    printLastError();
    return 1;
  }
  handle = ctx;

  objtype = "Context";
  CONTEXT_PROPERTIES;

  objtype = "AngularPannedSource";
  if (syz_createAngularPannedSource(&source, ctx, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, NULL, NULL, NULL) != 0) {
    printf("Couldn't create PannedSource ");
    printLastError();
    return 1;
  }
  handle = source;
  ANGULAR_PANNED_SOURCE_PROPERTIES;

  objtype = "BufferGenerator";
  if (syz_createBufferGenerator(&handle, ctx, NULL, NULL, NULL) != 0) {
    printf("Couldn't create BufferGenerator");
    printLastError();
    return 1;
  }
  if (syz_sourceAddGenerator(source, handle) != 0) {
    printf("Couldn't attach generator to source\n");
    printLastError();
    return 1;
  }

#undef DOUBLE_P
#define DOUBLE_P(...)
  BUFFER_GENERATOR_PROPERTIES;
#undef DOUBLE_P
#define DOUBLE_P(...) DOUBLE_P_IMPL(__VA_ARGS__)

  objtype = "Source3D";
  if (syz_createSource3D(&handle, ctx, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, 0.0, NULL, NULL, NULL) != 0) {
    printf("Couldn't create Source3D ");
    printLastError();
    return 1;
  }

  objtype = "FdnReverb";
  if (syz_createGlobalFdnReverb(&handle, ctx, NULL, NULL, NULL) != 0) {
    printf("Couldn't create GlobalFdnReverb ");
    printLastError();
    return 1;
  }

  syz_shutdown();
  return 0;
}
