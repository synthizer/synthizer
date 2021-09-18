#include "synthizer.h"

#include "synthizer/sources.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/panner_bank.hpp"

namespace synthizer {

AngularPannedSource::AngularPannedSource(const std::shared_ptr<Context> &ctx, int _panner_strategy)
    : PannedSource(ctx, _panner_strategy) {}

int AngularPannedSource::getObjectType() { return SYZ_OTYPE_ANGULAR_PANNED_SOURCE; }

void AngularPannedSource::preRun() {
  double azimuth, elevation;

  bool angles_changed = this->acquireAzimuth(azimuth) | this->acquireElevation(elevation);

  if (angles_changed) {
    this->panner_lane->setPanningAngles(azimuth, elevation);
  }
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createAngularPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                     void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  if (panner_strategy >= SYZ_PANNER_STRATEGY_COUNT) {
    throw ERange("Invalid panner strategy");
  }

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<AngularPannedSource>(panner_strategy);
  std::shared_ptr<Source> src_ptr = ret;
  ctx->registerSource(src_ptr);
  *out = toC(ret);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
