#include "synthizer.h"

#include "synthizer/sources/scalar_panned_source.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/panning/panner.hpp"

namespace synthizer {

ScalarPannedSource::ScalarPannedSource(const std::shared_ptr<Context> &ctx, int _panner_strategy)
    : PannedSource(ctx, _panner_strategy) {}

int ScalarPannedSource::getObjectType() { return SYZ_OTYPE_SCALAR_PANNED_SOURCE; }

void ScalarPannedSource::preRun() {
  double scalar;

  if (this->acquirePanningScalar(scalar)) {
    this->maybe_panner.value().setPanningScalar(scalar);
  }
}

} // namespace synthizer
