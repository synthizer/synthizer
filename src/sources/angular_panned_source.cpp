#include "synthizer.h"

#include "synthizer/sources/angular_panned_source.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/panning/panner.hpp"

namespace synthizer {

AngularPannedSource::AngularPannedSource(const std::shared_ptr<Context> &ctx, int _panner_strategy)
    : PannedSource(ctx, _panner_strategy) {}

int AngularPannedSource::getObjectType() { return SYZ_OTYPE_ANGULAR_PANNED_SOURCE; }

void AngularPannedSource::preRun() {
  double azimuth, elevation;

  bool angles_changed = this->acquireAzimuth(azimuth) | this->acquireElevation(elevation);

  if (angles_changed) {
    this->maybe_panner.value().setPanningAngles(azimuth, elevation);
  }
}

} // namespace synthizer
