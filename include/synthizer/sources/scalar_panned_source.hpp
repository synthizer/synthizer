#pragma once

#include "synthizer_constants.h"

#include "synthizer/property_internals.hpp"
#include "synthizer/source.hpp"
#include "synthizer/sources/panned_source.hpp"

#include <memory>

namespace synthizer {
class Context;

class ScalarPannedSource : public PannedSource {
public:
  ScalarPannedSource(const std::shared_ptr<Context> &ctx, int panner_strategy);

  int getObjectType() override;

  void preRun() override;

#define PROPERTY_CLASS ScalarPannedSource
#define PROPERTY_LIST SCALAR_PANNED_SOURCE_PROPERTIES
#define PROPERTY_BASE PannedSource
#include "synthizer/property_impl.hpp"
};

inline ScalarPannedSource::ScalarPannedSource(const std::shared_ptr<Context> &ctx, int _panner_strategy)
    : PannedSource(ctx, _panner_strategy) {}

inline int ScalarPannedSource::getObjectType() { return SYZ_OTYPE_SCALAR_PANNED_SOURCE; }

inline void ScalarPannedSource::preRun() {
  double scalar;

  if (this->acquirePanningScalar(scalar)) {
    this->maybe_panner.value().setPanningScalar(scalar);
  }
}

} // namespace synthizer