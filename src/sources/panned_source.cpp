#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/math.hpp"
#include "synthizer/panning/panner.hpp"
#include "synthizer/sources.hpp"
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
  int effective_panner_strategy = this->panner_strategy;
  if (effective_panner_strategy == SYZ_PANNER_STRATEGY_DELEGATE) {
    effective_panner_strategy = this->getContextRaw()->getDefaultPannerStrategy();
  }

  this->maybe_panner.emplace(buildPannerForStrategy(effective_panner_strategy));
}

void PannedSource::setGain3D(double gain) { this->gain_3d = gain; }

void PannedSource::run(unsigned int out_channels, float *out) {
  assert(out_channels == 2);

  auto &panner = this->maybe_panner.value();

  this->preRun();

  this->fillBlock(1);
  float *dest = panner.getInputBuffer();
  float g = this->gain_3d;
  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    dest[i] = this->block[i] * g;
  }

  panner.run(out_channels, out);
}

} // namespace synthizer
