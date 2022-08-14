#pragma once

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
  std::optional<Panner> maybe_panner;
};

} // namespace synthizer
