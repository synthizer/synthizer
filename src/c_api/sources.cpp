#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/sources/angular_panned_source.hpp"
#include "synthizer/sources/direct_source.hpp"
#include "synthizer/sources/scalar_panned_source.hpp"
#include "synthizer/sources/source_3d.hpp"

#include <memory>

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createDirectSource(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                              syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<DirectSource>();
  ctx->registerSource(ret);
  *out = toC(ret);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createAngularPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                     double azimuth, double elevation, void *config, void *userdata,
                                                     syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  if (panner_strategy >= SYZ_PANNER_STRATEGY_COUNT) {
    throw ERange("Invalid panner strategy");
  }
  if (azimuth < 0.0 || azimuth > 360.0) {
    throw ERange("Invalid azimuth");
  }
  if (elevation < -90.0 || elevation > 90.0) {
    throw ERange("Invalid elevation");
  }

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<AngularPannedSource>(panner_strategy);
  std::shared_ptr<Source> src_ptr = ret;
  ctx->registerSource(src_ptr);
  *out = toC(ret);

  auto e = syz_setD(*out, SYZ_P_AZIMUTH, azimuth);
  (void)e;
  assert(e == 0);
  e = syz_setD(*out, SYZ_P_ELEVATION, elevation);
  assert(e == 0);

  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}



SYZ_CAPI syz_ErrorCode syz_createScalarPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                    double panning_scalar, void *config, void *userdata,
                                                    syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  if (panner_strategy >= SYZ_PANNER_STRATEGY_COUNT) {
    throw ERange("Invalid panner strategy");
  }
  if (panning_scalar < -1.0 || panning_scalar > 1.0) {
    throw ERange("Invalid panning scalar");
  }

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<ScalarPannedSource>(panner_strategy);
  std::shared_ptr<Source> src_ptr = ret;
  ctx->registerSource(src_ptr);
  *out = toC(ret);

  auto e = syz_setD(*out, SYZ_P_PANNING_SCALAR, panning_scalar);
  (void)e;
  assert(e == 0);

  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}


SYZ_CAPI syz_ErrorCode syz_createSource3D(syz_Handle *out, syz_Handle context, int panner_strategy, double x, double y,
                                          double z, void *config, void *userdata,
                                          syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  if (panner_strategy >= SYZ_PANNER_STRATEGY_COUNT) {
    throw ERange("Invalid panner strategy");
  }

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<Source3D>(panner_strategy);
  std::shared_ptr<Source> src_ptr = ret;
  ctx->registerSource(src_ptr);
  *out = toC(ret);

  auto e = syz_setD3(*out, SYZ_P_POSITION, x, y, z);
  (void)e;
  assert(e == 0);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
