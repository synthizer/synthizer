#pragma once

#include "synthizer/filter_design.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/math.hpp"

#include <array>
#include <cassert>
#include <tuple>

namespace synthizer {

/*
 * A definition for a 3-band eq.
 * */
struct ThreeBandEqParams {
  /* Gains per band. */
  float dbgain_lower = 0.0f, dbgain_mid = 0.0f, dbgain_upper = 0.0f;
  /*
   * Frequencies for the eq. The bands are [0, lower), [lower, upper], (upper, nyquist]
   *
   * By default the middle band is from c4 to c6.
   * */
  float freq_lower = 261.0f, freq_upper = 1046.0f;
};

/*
 * A 3-band equalizer composed of a lowshelf and highshelf.
 * */
template <unsigned int LANES> class ThreeBandEq {
public:
  ThreeBandEq() { this->setParameters(ThreeBandEqParams{}); }

  void setParameters(const ThreeBandEqParams &params) {
    auto [l, u] = this->design(params);
    this->lower.setParameters(l);
    this->upper.setParameters(u);
  }

  void setParametersForLane(unsigned int lane, const ThreeBandEqParams &params) {
    assert(lane < LANES);
    auto [l, u] = this->design(params);
    this->lower.setParametersForLane(lane, l);
    this->upper.setParametersForLane(lane, u);
  }

  void tick(float *input, float *output) {
    std::array<float, LANES> intermediate = {{0.0f}};
    this->lower.tick(input, &intermediate[0]);
    this->upper.tick(&intermediate[0], output);
  }

  void reset() {
    this->lower.reset();
    this->upper.reset();
  }

private:
  /* Returns [lower, upper] */
  std::tuple<IIRFilterDef<3, 3>, IIRFilterDef<3, 3>> design(const ThreeBandEqParams &params) {
    /*
     * The overall gain is the gain for the middle band.
     * */
    double g = dbToGain(params.dbgain_mid);
    /*
     * The lower and upper shelf filters are designed to cut any extra gain in the middle band, then to apply
     * the user-requested gain for that band.
     * */
    float lg = params.dbgain_mid - params.dbgain_lower;
    float ug = params.dbgain_mid - params.dbgain_upper;
    auto l = designAudioEqLowShelf(params.freq_lower / config::SR, lg);
    auto u = designAudioEqHighShelf(params.freq_upper / config::SR, ug);
    /*
     * We want to save some work here, so instead of storing the gain ourselves, apply it to the first filter.
     * */
    l.gain *= g;
    return std::make_tuple(l, u);
  }

  IIRFilter<LANES, 3, 3> lower, upper;
  std::array<float, LANES> gain;
};

} // namespace synthizer
