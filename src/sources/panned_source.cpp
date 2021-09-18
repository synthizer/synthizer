#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/sources.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace synthizer {

PannedSource::PannedSource(const std::shared_ptr<Context> context, int _panner_strategy)
    : Source(context), panner_strategy(_panner_strategy) {}

void PannedSource::initInAudioThread() {
  Source::initInAudioThread();
  // Copy the default in from the context, if it wasn't overridden.
  if (this->panner_strategy == SYZ_PANNER_STRATEGY_DELEGATE) {
    this->panner_strategy = this->getContextRaw()->getDefaultPannerStrategy();
  }

  this->panner_lane = this->context->allocateSourcePannerLane((enum SYZ_PANNER_STRATEGY)this->panner_strategy);

  /*
   * PannedSource uses the changed status of properties to determine whether or not the user
   * is using azimuth/elevation or the panning scalar. Since the panner's default config is okay, mark them all
   * unchanged and wait for the user to set one.
   * */
  this->markAzimuthUnchanged();
  this->markElevationUnchanged();
  this->markPanningScalarUnchanged();
}

int PannedSource::getObjectType() { return SYZ_OTYPE_PANNED_SOURCE; }

void PannedSource::setGain3D(double gain) { this->gain_3d = gain; }

void PannedSource::run() {
  double azimuth, elevation, panning_scalar;

  bool angles_changed = this->acquireAzimuth(azimuth) | this->acquireElevation(elevation);
  bool scalar_changed = this->acquirePanningScalar(panning_scalar);

  if (angles_changed) {
    this->panner_lane->setPanningAngles(azimuth, elevation);
  } else if (scalar_changed) {
    this->panner_lane->setPanningScalar(panning_scalar);
  }

  this->fillBlock(1);
  /* And output. */
  this->panner_lane->update();
  unsigned int stride = this->panner_lane->stride;
  float *dest = this->panner_lane->destination;
  float g = this->gain_3d;
  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    dest[i * stride] = g * block[i];
  }
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy, void *userdata,
                                              syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  if (panner_strategy >= SYZ_PANNER_STRATEGY_COUNT) {
    throw ERange("Invalid panner strategy");
  }

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<PannedSource>(panner_strategy);
  std::shared_ptr<Source> src_ptr = ret;
  ctx->registerSource(src_ptr);
  *out = toC(ret);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
