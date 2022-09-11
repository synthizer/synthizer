#pragma once

#include "synthizer_constants.h"

#include "synthizer/error.hpp"
#include "synthizer/panning/hrtf_panner.hpp"
#include "synthizer/panning/stereo_panner.hpp"
#include "synthizer/types.hpp"

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace synthizer {
using PannerVariant = std::variant<StereoPanner, HrtfPanner>;
static_assert(std::is_move_constructible<PannerVariant>::value);

class Panner {
public:
  Panner() = delete;
  Panner(const Panner &) = delete;
  Panner(Panner &&) = default;

  /**
   * Get the mono input buffer, one BLOCK_SIZE in length.
   *
   * This pointer is only valid until the next call to run.
   * */
  float *getInputBuffer();

  /**
   * Get the number of output channels for this panner.
   *
   * This value never changes for the lifetime of the panner.
   * */
  unsigned int getOutputChannelCount();

  /**
   * Run the panner, producing one block of audio to the specified destination.
   * */
  void run(unsigned int out_channels, float *out);

  /**
   * Set the panner's position as an azimuth and elevation pair.
   *
   * Azimuth and elevation are in degrees.  Azimuth moves clockwise with 0 as straight in front and elevation moves
   * bottom to top with -90 as down.  Azimuth must be in the range 0 <= azimuth <= 360 and elevation in the range -90 <=
   * elevation <= 90.  These conditions are slightly odd because this is what HRTF datasets and audio papers like.
   * */
  void setPanningAngles(double azimuth, double elevation);

  /**
   * Set the position of the panner as a scalar from -1.0 (left) to 1.0 (right).
   * */
  void setPanningScalar(double scalar);

private:
// GCC cannot seemingly understand that the following constructor is fine and insists that (1) there is a temporary and
// (2) that temporary is uninitialized.
//
// We could try to fix it but the only fix is going to require possible copies of objects on the order of 100kb because
// apparently GCC will only be happy if this doesn't involve a reference, at all.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  Panner(PannerVariant &&pv) : implementation(pv) {}
#pragma GCC diagnostic pop

  PannerVariant implementation;

  friend Panner buildPannerForStrategy(int panner_strategy);
};

inline float *Panner::getInputBuffer() {
  return std::visit([](auto &&v) { return v.getInputBuffer(); }, this->implementation);
}

inline unsigned int Panner::getOutputChannelCount() {
  return std::visit([](auto &&v) { return v.getOutputChannelCount(); }, this->implementation);
}

inline void Panner::setPanningAngles(double azimuth, double elevation) {
  std::visit([&](auto &&v) { v.setPanningAngles(azimuth, elevation); }, this->implementation);
}

inline void Panner::setPanningScalar(double scalar) {
  std::visit([&](auto &&v) { v.setPanningScalar(scalar); }, this->implementation);
}

inline void Panner::run(unsigned int out_channels, float *out) {
  (void)out_channels;

  /* for now Synthizer only ever works with stereo audio. */
  assert(out_channels == 2);
  assert(this->getOutputChannelCount() == 2);

  std::visit([&](auto &&v) { v.run(out); }, this->implementation);
}

inline Panner buildPannerForStrategy(int panner_strategy) {
  switch (panner_strategy) {
  case SYZ_PANNER_STRATEGY_STEREO:
    return Panner(PannerVariant(StereoPanner()));
  case SYZ_PANNER_STRATEGY_HRTF:
    return Panner(PannerVariant(HrtfPanner()));
  default:
    throw ENotSupported("This panner type is not supported");
  }
}

} // namespace synthizer
