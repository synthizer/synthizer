#pragma once

#include "synthizer/property_internals.hpp"
#include "synthizer/source.hpp"
#include "synthizer_constants.h"

#include <memory>

namespace synthizer {
class Context;

class DirectSource : public Source {
public:
  DirectSource(std::shared_ptr<Context> ctx) : Source(ctx) {}

  int getObjectType() override;
  void run(unsigned int out_channels, float *out) override;
};

inline int DirectSource::getObjectType() { return SYZ_OTYPE_DIRECT_SOURCE; }

inline void DirectSource::run(unsigned int out_channels, float *out) {
  /*
   * For now, use the fact that we know that the context is always stereo.
   * In future this'll need to be done better.
   * */
  assert(out_channels == 2);
  this->fillBlock(out_channels);

  for (unsigned int i = 0; i < config::BLOCK_SIZE * out_channels; i++) {
    out[i] += this->block[i];
  }
}

} // namespace synthizer
