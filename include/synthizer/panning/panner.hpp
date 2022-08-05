#pragma once

#include "synthizer/panning/hrtf_panner.hpp"
#include "synthizer/panning/stereo_panner.hpp"

#include "synthizer/types.hpp"
#include "synthizer_constants.h"

#include <memory>
#include <tuple>

namespace synthizer {

/*
 * This is the panner bank infrastructure. Panner banks hold multiple copies of panners, and run them in parallel.
 *
 * */

/*
 * A lane of a panner. Stride is the distance between the elements, and destination the place to copy to.
 *
 * It is necessary to call update on every tick from somewhere in order to refresh stride and destination.
 * */
class PannerLane {
public:
  virtual ~PannerLane() {}
  virtual void update() = 0;

  unsigned int stride = 1;
  float *destination = nullptr;

  virtual void setPanningAngles(double azimuth, double elvation) = 0;
  virtual void setPanningScalar(double scalar) = 0;
};
} // namespace synthizer