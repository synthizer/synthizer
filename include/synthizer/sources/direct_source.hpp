#pragma once

#include "synthizer/property_internals.hpp"
#include "synthizer/source.hpp"

#include <memory>

namespace synthizer {
class Context;

class DirectSource : public Source {
public:
  DirectSource(std::shared_ptr<Context> ctx) : Source(ctx) {}

  int getObjectType() override;
  void run(unsigned int out_channels, float *out) override;
};

} // namespace synthizer
