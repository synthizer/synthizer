#pragma once

#include "synthizer/config.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * base class for effects, which can be used either with generators (tbd) or as global effects to which sources are
 * connected. This cannot have properties on it.
 *
 * An effect needs to implement this interface, then declare up to 2 versions of itself by subclassing one level
 * further. See echo.{hpp|cpp} for a worked example.
 *
 * Notice that this doesn't inherit from BaseObject: whether or not this is integrated with the routing infrastructure
 * and how needs to be left up to the concrete implementations.  Usually, this is accomplished via using this as a
 * mixin.
 * */
class BaseEffect {
public:
  virtual ~BaseEffect() {}

  /*
   * Always processes exactly one block.
   *
   * inputChannels may be known by the effect. When this is the case, it is acceptable to ignore that argument for the
   * sake of loop unrolling, etc.
   *
   * It is also acceptable not to write to the output argument if the effect is, i.e, using PannerBank infrastructure to
   * share panning costs. In that case, however, the implementation can't play nice with generators in future when
   * that's added.
   *
   * time_in_blocks exists to allow for crossfading, without us slowly accumulating a bunch of clocks everywhere.
   *
   * The gain argument should be applied by the effect.  It's folded in to avoid the need for further temporary buffers.
   * */
  virtual void runEffect(unsigned int time_in_blocks, unsigned int input_channels, float *input,
                         unsigned int output_channels, float *output, float gain) = 0;
  virtual void resetEffect() = 0;

  /**
   * All derived effects must know how long they wish to linger for. This is not optional.
   *
   * The actual lingering behavior is handled in e.g. GlobalEffect.
   * */
  virtual double getEffectLingerTimeout() = 0;
};

} // namespace synthizer
