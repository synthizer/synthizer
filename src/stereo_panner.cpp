#include "synthizer/stereo_panner.hpp"
#include "synthizer/math.hpp"

#include <array>
#include <cassert>
#include <math.h>
#include <tuple>

namespace synthizer {

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
