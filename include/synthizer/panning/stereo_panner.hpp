#pragma once

#include "synthizer/config.hpp"
#include "synthizer/math.hpp"

#include "synthizer/types.hpp"

#include <array>
#include <tuple>

namespace synthizer {

class StereoPanner {
public:
  static constexpr unsigned int CHANNELS = 2;

  StereoPanner() { this->setPanningScalar(0.0); }

  unsigned int getOutputChannelCount();
  float *getInputBuffer();
  void run(float *output);
  void setPanningAngles(double azimuth, double elevation);
  void setPanningScalar(double scalar);

private:
  std::array<float, config::BLOCK_SIZE> block{0.0};
  float gain_l, gain_r;
};

inline unsigned int StereoPanner::getOutputChannelCount() { return CHANNELS; }

inline float *StereoPanner::getInputBuffer() { return &this->block[0]; }

inline void StereoPanner::run(float *output) {
  for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
    output[i * 2] += this->gain_l * this->block[i];
    output[i * 2 + 1] += this->gain_r * this->block[i];
  }
}

inline void StereoPanner::setPanningAngles(double azimuth, double elevation) {
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
  this->setPanningScalar(scalar);
}

inline void StereoPanner::setPanningScalar(double scalar) {
  /*
   * Transform from [ -1, 1] to [ 0, 1 ]
   * Then from [0, 1] to [0, 90]
   * Then do standard constant power panning with the trig identity:
   * cos(x)^2+sin(x^2) = 1.
   * Remember that the amplitudes are square root of power, so it's just cos(x) and sin(x).
   * */
  double angle = (1.0 + scalar) / 2.0 * 90.0;
  angle *= PI / 180.0;
  this->gain_l = cosf(angle);
  this->gain_r = sinf(angle);
}

} // namespace synthizer