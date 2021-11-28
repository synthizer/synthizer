#pragma once

/** A sine bank based off the following identities:
 *
 * ```
 * sin(a + b) = sin(a) * cos(b) + cos(a) * sin(b);
 * cos(a + b) = cos(a) * cos(b) - sin(a) * sin(b)
 * ```
 *
 * By holding b constant, we can do the entire thing without any actual trig functions after initialization.  That is a
 * is the advancing/output angle.
 *
 * This uses templates for autovectorization of computation, primarily (but not only) for multiples of 4.  To keep
 * accuracy, it resynchronizes each time `fillBlock` is called.
 *
 * SR is hardcoded at the Synthizer samplerate, as per usual.
 * */

#include <algorithm>
#include <math.h> // Older gcc doesn't like cmath because sinf and cosf don't exist.
#include <vector>

#include "synthizer/config.hpp"
#include "synthizer/math.hpp"
#include "synthizer/memory.hpp"

namespace synthizer {
/**
 * Configuration for an individual sine wave.
 * */
class SineWaveConfig {
public:
  SineWaveConfig(double _freq_mul, double _phase, double _gain) : phase(_phase), freq_mul(_freq_mul), gain(_gain) {}
  /* Phase, in the range 0.0 to 1.0. */
  double phase;
  /* Frequency, as a multiplier of the base frequency. E.g 1.0 is the base frequency, 2.0 the first harmonic. */
  double freq_mul;
  double gain;
};

class FastSineBank {
public:
  FastSineBank(double _frequency);
  void addWave(const SineWaveConfig &wave);
  void clearWaves();
  void setFrequency(double frequency);

  /* Fill a block of `SAMPLES` length with the output from this generator. */
  template <std::size_t SAMPLES, bool ADD = true> void fillBlock(float *out);

private:
  template <std::size_t SAMPLES, std::size_t WAVES> void fillBlockHelper(float *out, std::size_t start);
  deferred_vector<SineWaveConfig> waves;
  double frequency;

  // To allow for frequency changes without artifacts or fading, we must store time on a per-wave basis.
  class WaveState {
  public:
    WaveState(double _time) : time(_time) {}

    // Time for this wave specifically in the range 0.0 to 1.0.
    double time = 0.0;
  };

  std::vector<WaveState> wave_states;
};

FastSineBank::FastSineBank(double _frequency) : frequency(_frequency) {}

template <std::size_t SAMPLES, std::size_t WAVES> void FastSineBank::fillBlockHelper(float *out, std::size_t start) {
  /* sa=sin(a), ca=cos(a), etc. */
  float sa[WAVES], ca[WAVES], sb[WAVES], cb[WAVES], gains[WAVES];

  // Initialize our variables using real values.
  for (std::size_t i = 0; i < WAVES; i++) {
    std::size_t wave_ind = i + start;
    double freq = this->waves[wave_ind].freq_mul * this->frequency;
    double t = 2 * PI * this->wave_states[wave_ind].time;
    // Advance the wave's time.
    this->wave_states[wave_ind].time =
        fmod(this->wave_states[wave_ind].time + freq * (SAMPLES / (double)config::SR), 1.0);

    sa[i] = sinf(t);
    ca[i] = cosf(t);
    // How many radians does this wave advance by per sample?
    double b = 2 * PI * freq / config::SR;
    sb[i] = sinf(b);
    cb[i] = cosf(b);
    gains[i] = waves[wave_ind].gain;
  }

  for (std::size_t s = 0; s < SAMPLES; s++) {
    for (std::size_t i = 0; i < WAVES; i++) {
      float new_sa = sa[i] * cb[i] + ca[i] * sb[i];
      float new_ca = ca[i] * cb[i] - sa[i] * sb[i];
      out[s] += gains[i] * sa[i];
      sa[i] = new_sa;
      ca[i] = new_ca;
    }
  }
}

template <std::size_t SAMPLES, bool ADD> void FastSineBank::fillBlock(float *out) {
  if (ADD == false) {
    std::fill(out, out + SAMPLES, 0.0);
  }

  // break this down by 16, 8, 4, 2, 1.  Gives the compiler lots of autovectorization room.
  std::size_t i = 0;
#define BLOCK(X)                                                                                                       \
  for (; i < this->waves.size() / X * X; i += X) {                                                                     \
    this->fillBlockHelper<SAMPLES, X>(out, i);                                                                         \
  }

  BLOCK(32)
  BLOCK(16)
  BLOCK(8)
  BLOCK(4)
  BLOCK(1)

#undef BLOCK
}

void FastSineBank::addWave(const SineWaveConfig &wave) {
  this->waves.push_back(wave);
  this->wave_states.emplace_back(FastSineBank::WaveState{wave.phase});
}

void FastSineBank::clearWaves() {
  this->waves.clear();
  this->wave_states.clear();
}

void FastSineBank::setFrequency(double _frequency) { this->frequency = _frequency; }

} // namespace synthizer
