#pragma once

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

} // namespace synthizer
