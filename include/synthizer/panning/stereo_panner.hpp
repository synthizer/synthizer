#pragma once

#include "synthizer/config.hpp"
#include "synthizer/panning/panner_bank.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <tuple>

namespace synthizer {

class StereoPanner {
public:
  static constexpr unsigned int CHANNELS = 2;
  static constexpr unsigned int LANES = 4;

  unsigned int getOutputChannelCount();
  unsigned int getLaneCount();
  void run(float *output);
  std::tuple<float *, unsigned int> getLane(unsigned int lane);
  void releaseLane(unsigned int lane);
  void setPanningAngles(unsigned int lane, double azimuth, double elevation);
  void setPanningScalar(unsigned int lane, double scalar);

private:
  std::array<float, LANES * config::BLOCK_SIZE> data{0.0};
  /*
   * Store the output gains as { l1, r1, l2, r2, l3, r3, l4, r4 }
   *  */
  std::array<float, LANES * 2> gains;
};

unsigned int StereoPanner::getOutputChannelCount() { return CHANNELS; }

unsigned int StereoPanner::getLaneCount() { return LANES; }

void StereoPanner::run(float *output) {
  for (unsigned int ch = 0; ch < LANES * 2; ch++) {
    float *d = &this->data[ch / 2];
    float *o = output + ch;
    float g = this->gains[ch];
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      o[i * LANES * CHANNELS] += g * d[i * LANES];
    }
  }
}

std::tuple<float *, unsigned int> StereoPanner::getLane(unsigned int lane) {
  assert(lane < LANES);
  return {&this->data[lane], LANES};
}

void StereoPanner::releaseLane(unsigned int lane) {
  assert(lane < LANES);

  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    this->data[i * LANES + lane] = 0.0f;
  }
}

void StereoPanner::setPanningAngles(unsigned int lane, double azimuth, double elevation) {
  (void)elevation;

  /*
   * Angles are clockwise from forward; we want clockwise from left.
   * Then if we wrap, drop the wrapped part.
   * */
  double angle = fmod(90.0 + azimuth, 360.0);
  double scalar;
  if (angle <= 180) {
    scalar = -1.0 + 2.0 * (angle / 180.0);
  } else {
    scalar = 1.0 - 2.0 * (angle - 180) / 180.0;
  }
  this->setPanningScalar(lane, scalar);
}

void StereoPanner::setPanningScalar(unsigned int lane, double scalar) {
  assert(lane < LANES);

  /*
   * Transform from [ -1, 1] to [ 0, 1 ]
   * Then from [0, 1] to [0, 90]
   * Then do standard constant power panning with the trig identity:
   * cos(x)^2+sin(x^2) = 1.
   * Remember that the amplitudes are square root of power, so it's just cos(x) and sin(x).
   * */
  double angle = (1.0 + scalar) / 2.0 * 90.0;
  angle *= PI / 180.0;
  float left = cosf(angle);
  float right = sinf(angle);
  this->gains[lane * 2] = left;
  this->gains[lane * 2 + 1] = right;
}

} // namespace synthizer