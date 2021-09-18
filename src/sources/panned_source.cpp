#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/sources.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace synthizer {

PannedSource::PannedSource(const std::shared_ptr<Context> context, int _panner_strategy)
    : Source(context), panner_strategy(_panner_strategy) {}

void PannedSource::initInAudioThread() {
  Source::initInAudioThread();
  // Copy the default in from the context, if it wasn't overridden.
  if (this->panner_strategy == SYZ_PANNER_STRATEGY_DELEGATE) {
    this->panner_strategy = this->getContextRaw()->getDefaultPannerStrategy();
  }

  this->panner_lane = this->context->allocateSourcePannerLane((enum SYZ_PANNER_STRATEGY)this->panner_strategy);
}

void PannedSource::setGain3D(double gain) { this->gain_3d = gain; }

void PannedSource::run() {
  this->preRun();

  this->fillBlock(1);
  /* And output. */
  this->panner_lane->update();
  unsigned int stride = this->panner_lane->stride;
  float *dest = this->panner_lane->destination;
  float g = this->gain_3d;
  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    dest[i * stride] = g * block[i];
  }
}

} // namespace synthizer
