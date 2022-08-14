#pragma once

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

} // namespace synthizer