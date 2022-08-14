#pragma once

#include "synthizer_constants.h"

#include "synthizer/context.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/sources/angular_panned_source.hpp"

#include <memory>

namespace synthizer {
class Context;

class Source3D : public AngularPannedSource {
public:
  Source3D(std::shared_ptr<Context> context, int panner_strategy);
  void initInAudioThread() override;

  int getObjectType() override;
  void preRun() override;
  // No run: PannedSource already handles that for us.

#define PROPERTY_CLASS Source3D
#define PROPERTY_BASE PannedSource
#define PROPERTY_LIST SOURCE3D_PROPERTIES
#include "synthizer/property_impl.hpp"
};

inline Source3D::Source3D(std::shared_ptr<Context> context, int _panner_strategy)
    : AngularPannedSource(context, _panner_strategy) {}

inline void Source3D::initInAudioThread() {
  PannedSource::initInAudioThread();
  setPropertiesFromDistanceParams(this, this->getContext()->getDefaultDistanceParams());
}

inline int Source3D::getObjectType() { return SYZ_OTYPE_SOURCE_3D; }

inline void Source3D::preRun() {
  auto ctx = this->getContext();
  auto listener_pos = ctx->getPosition();
  auto listener_orient = ctx->getOrientation();
  Vec3d listener_at = {listener_orient[0], listener_orient[1], listener_orient[2]};
  Vec3d listener_up = {listener_orient[3], listener_orient[4], listener_orient[5]};

  auto position_prop = this->getPosition();
  Vec3d pos = {position_prop[0] - listener_pos[0], position_prop[1] - listener_pos[1],
               position_prop[2] - listener_pos[2]};
  Vec3d at = normalize(listener_at);
  Vec3d right = normalize(crossProduct(listener_at, listener_up));
  Vec3d up = crossProduct(right, at);

  /* Use the above to go to a coordinate system where positive y is forward, positive x is right, positive z is up. */
  double y = dotProduct(at, pos);
  double x = dotProduct(right, pos);
  double z = dotProduct(up, pos);

  /* Now get spherical coordinates. */
  double dist = magnitude(pos);
  if (dist == 0) {
    /* Source is at the center of the head. Arbitrarily decide on forward. */
    x = 0;
    y = 1;
    z = 0;
  } else {
    x /= dist;
    y /= dist;
    z /= dist;
  }
  /* Remember: this is clockwise of north and atan2 is -pi to pi. */
  double azimuth = std::atan2(x, y);
  double elevation = std::atan2(z, std::sqrt(x * x + y * y));

  azimuth = std::fmod(azimuth * 180 / PI + 360, 360.0);
  this->setAzimuth(clamp(azimuth, 0.0, 360.0));
  elevation = clamp(elevation * 180 / PI, -90.0, 90.0);
  this->setElevation(elevation);
  auto dp = materializeDistanceParamsFromProperties(this);
  dp.distance = dist;
  this->setGain3D(mulFromDistanceParams(dp));
  assert(azimuth >= 0.0);
  AngularPannedSource::preRun();
}

} // namespace synthizer
