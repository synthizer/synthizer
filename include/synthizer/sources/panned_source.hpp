#pragma once

#include "synthizer_constants.h"

#include "synthizer/source.hpp"

#include <memory>

namespace synthizer {
class Context;

/*
 * A PannedSource is controlled through azimuth/elevation or panning scalar plus a gain factor.
 * */
class PannedSource : public Source {
public:
  PannedSource(std::shared_ptr<Context> context, int panner_strategy);
  void initInAudioThread() override;

  void run(unsigned int out_channels, float *out) override;

protected:
  unsigned int panner_strategy;
  /**
   * So this is a bit annoying.  We need to only initialize the panner once in the audio thread in case the user
   * delegated the panning strategy because this is the only way to maintain the total order.  Rather than being
   * complicated in that API where the logic is, just make sources deal.
   * */
  std::optional<Panner> maybe_panner = std::nullopt;
};

inline PannedSource::PannedSource(const std::shared_ptr<Context> context, int _panner_strategy)
    : Source(context), panner_strategy(_panner_strategy) {}

inline void PannedSource::initInAudioThread() {
  int effective_panner_strategy = this->panner_strategy;
  if (effective_panner_strategy == SYZ_PANNER_STRATEGY_DELEGATE) {
    effective_panner_strategy = this->getContextRaw()->getDefaultPannerStrategy();
  }

  this->maybe_panner.emplace(buildPannerForStrategy(effective_panner_strategy));
}

inline void PannedSource::run(unsigned int out_channels, float *out) {
  assert(out_channels == 2);

  auto &panner = this->maybe_panner.value();

  this->fillBlock(1);
  float *dest = panner.getInputBuffer();

  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    dest[i] = this->block[i];
  }

  panner.run(out_channels, out);
}

} // namespace synthizer
