#pragma once

#include "synthizer_constants.h"

#include "synthizer/property_internals.hpp"
#include "synthizer/sources/panned_source.hpp"

#include <memory>

namespace synthizer {
class Context;

class AngularPannedSource : public PannedSource {
public:
  AngularPannedSource(const std::shared_ptr<Context> &ctx, int panner_strategy);

  int getObjectType() override;

  void preRun() override;

#define PROPERTY_CLASS AngularPannedSource
#define PROPERTY_LIST ANGULAR_PANNED_SOURCE_PROPERTIES
#define PROPERTY_BASE PannedSource
#include "synthizer/property_impl.hpp"
};

inline AngularPannedSource::AngularPannedSource(const std::shared_ptr<Context> &ctx, int _panner_strategy)
    : PannedSource(ctx, _panner_strategy) {}

inline int AngularPannedSource::getObjectType() { return SYZ_OTYPE_ANGULAR_PANNED_SOURCE; }

inline void AngularPannedSource::preRun() {
  double azimuth, elevation;

  bool angles_changed = this->acquireAzimuth(azimuth) | this->acquireElevation(elevation);

  if (angles_changed) {
    this->maybe_panner.value().setPanningAngles(azimuth, elevation);
  }
}

} // namespace synthizer