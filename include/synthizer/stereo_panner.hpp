#pragma once

#include "synthizer/config.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <tuple>

namespace synthizer {

class StereoPanner : public AbstractPanner {
public:
  static constexpr unsigned int CHANNELS = 2;
  static constexpr unsigned int LANES = 4;

  unsigned int getOutputChannelCount() override;
  unsigned int getLaneCount() override;
  void run(float *output) override;
  std::tuple<float *, unsigned int> getLane(unsigned int lane) override;
  void releaseLane(unsigned int lane) override;
  void setPanningAngles(unsigned int lane, double azimuth, double elevation) override;
  void setPanningScalar(unsigned int lane, double scalar) override;

private:
  std::array<float, LANES * config::BLOCK_SIZE> data{0.0};
  /*
   * Store the output gains as { l1, r1, l2, r2, l3, r3, l4, r4 }
   *  */
  std::array<float, LANES * 2> gains;
};

} // namespace synthizer