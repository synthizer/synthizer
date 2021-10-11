#include "synthizer.h"

#include "synthizer/sources.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/panner_bank.hpp"

namespace synthizer {

ScalarPannedSource::ScalarPannedSource(const std::shared_ptr<Context> &ctx, int _panner_strategy)
    : PannedSource(ctx, _panner_strategy) {}

int ScalarPannedSource::getObjectType() { return SYZ_OTYPE_SCALAR_PANNED_SOURCE; }

void ScalarPannedSource::preRun() {
  double scalar;

  if (this->acquirePanningScalar(scalar)) {
    this->panner_lane->setPanningScalar(scalar);
  }
}

} // namespace synthizer

using namespace synthizer;

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
