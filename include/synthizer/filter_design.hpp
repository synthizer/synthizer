#pragma once

#include "synthizer.h"

#include "synthizer/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
#include <type_traits>

namespace synthizer {

/* Holds filter coefficients for an IIR filter. */
template <std::size_t num, std::size_t den, typename enabled = void> class IIRFilterDef;

template <std::size_t num, std::size_t den> class IIRFilterDef<num, den, typename std::enable_if<(num > 0)>::type> {
public:
  /*
   * Numerator of the filter (b_x in audio EQ cookbook).
   * */
  std::array<double, num> num_coefs;
  /*
   * Denominator of the filter (a_x in audio EQ cookbook).
   * First coefficient is implicit and always 1.
   * */
  std::array<double, den == 0 ? 0 : den - 1> den_coefs;
  /*
   * All filters are normalized so that a0 = 1.
   *
   * This is the missing gain factor that needs to be added back in.
   * */
  double gain;

  template <std::size_t NN, std::size_t ND, typename RES = typename std::enable_if_t<NN <= num && ND <= den, void>>
  auto operator=(const IIRFilterDef<NN, ND> &other) -> RES {
    this->num_coefs.fill(0.0);
    this->den_coefs.fill(0.0);

    if (num != 0) {
      for (std::size_t i = 0; i < NN; i++) {
        this->num_coefs[i] = other.num_coefs[i];
      }
    }

    if (den != 0) {
      for (std::size_t i = 0; i < ND; i++) {
        this->den_coefs[i] = other.den_coefs[i];
      }
    }

    this->gain = other.gain;
  }

  template <std::size_t NN, std::size_t ND> bool operator==(const IIRFilterDef<NN, ND> &other) {
    if (NN > num || ND > den) {
      return false;
    }

    if (this->gain != other.gain) {
      return false;
    }

    for (std::size_t i = 0; i < num; i++) {
      double nv = i >= NN ? 0.0 : other.num_coefs[i];
      if (this->num_coefs[i] != nv) {
        return false;
      }
    }

    /* For this loop, remember that the leading coefficients are implicit. */
    for (std::size_t i = 0; i < den - 1; i++) {
      double nv = i >= ND - 1 ? 0.0 : other.den_coefs[i];
      if (this->den_coefs[i] != nv) {
        return false;
      }
    }

    return true;
  }

  template <std::size_t NN, std::size_t ND> bool operator!=(const IIRFilterDef<NN, ND> &other) {
    return !(*this == other);
  }
};

template <std::size_t num1, std::size_t den1, std::size_t num2, std::size_t den2>
IIRFilterDef<num1 + num2 - 1, den1 + den2 - 1> combineIIRFilters(IIRFilterDef<num1, den1> f1,
                                                                 IIRFilterDef<num2, den2> f2) {
  double newGain = f1.gain * f2.gain;
  std::array<double, num1 + num2 - 1> workingNum;
  std::array<double, den1 + den2 - 1> workingDen;
  std::fill(workingNum.begin(), workingNum.end(), 0.0);
  std::fill(workingDen.begin(), workingDen.end(), 0.0);

  /*
   * Convolve the numerators, then convolve the denominators.
   * */
  for (unsigned int i = 0; i < num1; i++) {
    double num1r = f1.num_coefs[i];
    for (unsigned int j = 0; j < num2; j++) {
      double num2r = f2.num_coefs[j];
      workingNum[i + j] += num1r * num2r;
    }
  }

  for (unsigned int i = 0; i < den1; i++) {
    double den1r = i == 0 ? 1.0 : f1.den_coefs[i - 1];
    for (unsigned int j = 0; j < den2; j++) {
      double den2r = j == 0 ? 1.0 : f2.den_coefs[j - 1];
      workingDen[i + j] += den1r * den2r;
    }
  }

  IIRFilterDef<num1 + num2 - 1, den1 + den2 - 1> ret;
  ret.gain = newGain;
  std::copy(workingNum.begin(), workingNum.end(), ret.num_coefs.begin());
  std::copy(workingDen.begin() + 1, workingDen.end(), ret.den_coefs.begin());
  return ret;
}

/*
 * A wire is the identity filter.
 * */
inline IIRFilterDef<1, 0> designWire() {
  IIRFilterDef<1, 0> def;
  def.gain = 1.0;
  def.num_coefs[0] = 1.0;
  return def;
}

namespace simple_filters_detail {
/*
 * Compute coefficients for a zero of the polynomial a0+a1 z^-1
 *
 * a1 = -x because a0 =1  in all cases, so
 * 1 + a1 z^-1 = 0
 * 1 = -a1 z^-1
 * 1 = -a1 / z
 * z = -a1
 * */
inline std::tuple<double, double> coefsForZero(double x) { return {1, -x}; }
} // namespace simple_filters_detail

/*
 * A single-zero filter.
 * Zero is on the x axis.
 * Normalized so that peak gain is 1.
 * */
inline IIRFilterDef<2, 1> designOneZero(double zero) {
  double b0, b1;
  std::tie(b0, b1) = simple_filters_detail::coefsForZero(zero);
  IIRFilterDef<2, 1> ret;
  ret.num_coefs = {b0, b1};
  ret.gain = 1.0 / (abs(b0) + abs(b1));
  ret.den_coefs = {};
  return ret;
}

/*
 * A single-pole filter.
 * Normalized so that peak gain is 1.
 * */
inline IIRFilterDef<1, 2> designOnePole(double pole) {
  double a0, a1;
  // poles are zero in the denominator.
  std::tie(a0, a1) = simple_filters_detail::coefsForZero(pole);
  IIRFilterDef<1, 2> ret;
  ret.num_coefs = {1.0};
  ret.den_coefs = {a1};
  /*
   * Explanation: the gain is maximum at either:
   * -1, because a1 > 0, giving 1/(1-a0)
   * 1, because a1 < 0, giving 1/(1-abs(a0))
   * The former expression can have abs added without changing its value, so we avoid the conditional.
   * */
  ret.gain = 1 - abs(a1);
  return ret;
}

inline IIRFilterDef<2, 2> designDcBlocker(double r = 0.995) {
  auto num = designOneZero(1);
  auto den = designOnePole(r);
  return combineIIRFilters(num, den);
}

/* Coefficients for a 2-pole 2-zero filter, often from the audio eq cookbook. */
using BiquadFilterDef = IIRFilterDef<3, 3>;

/*
 * Implement the cases from the audio EQ cookbook, in the root of this repository.
 *
 * Instead of frequency we use omega, which is frequency/sr.
 * This is because not all of the library operates at config::SR (i.e. oversampled effects).
 *
 * For lowpass and highpass, default q gives butterworth polynomials in the denominator.
 * */
namespace biquad_design_detail {
inline BiquadFilterDef makeRet(double b0, double b1, double b2, double a0, double a1, double a2) {
  BiquadFilterDef ret;
  const double gain = 1 / a0;
  a1 /= a0;
  a2 /= a0;

  ret.num_coefs = {b0, b1, b2};
  ret.den_coefs = {a1, a2};
  ret.gain = gain;
  return ret;
}
} // namespace biquad_design_detail

#define BIQUAD_RET return biquad_design_detail::makeRet(b0, b1, b2, a0, a1, a2);
#define BIQUAD_VAR_W0 const double w0 = 2 * PI * omega
#define BIQUAD_VAR_CW0 const double cw0 = cos(w0)
#define BIQUAD_VAR_SW0 const double sw0 = sin(w0)

inline BiquadFilterDef designAudioEqLowpass(double omega, double q = 0.7071135624381276) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_SW0;
  BIQUAD_VAR_CW0;

  const double alpha = sw0 / (2 * q);

  const double b0 = (1 - cw0) / 2;
  const double b1 = 1 - cw0;
  const double b2 = b1 / 2;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  BIQUAD_RET;
}

inline BiquadFilterDef designAudioEqHighpass(double omega, double q = 0.7071135624381276) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_CW0;
  BIQUAD_VAR_SW0;

  const double alpha = sw0 / (2 * q);

  const double b0 = (1 + cw0) / 2;
  const double b1 = -(1 + cw0);
  const double b2 = (1 + cw0) / 2;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;

  BIQUAD_RET;
}

inline BiquadFilterDef designAudioEqBandpass(double omega, double bw) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_SW0;
  BIQUAD_VAR_CW0;

  const double alpha = sw0 * sinh(std::log(2) / 2 * bw * omega / sw0);
  const double b0 = alpha;
  const double b1 = 0;
  const double b2 = -alpha;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  BIQUAD_RET;
}

/* Aka band reject, but we use audio eq terminology. */
inline BiquadFilterDef designAudioEqNotch(double omega, double bw) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_SW0;
  BIQUAD_VAR_CW0;

  const double alpha = sw0 * sinh(std::log(2) / 2 * bw * omega / sw0);

  const double b0 = 1;
  const double b1 = -2 * cw0;
  const double b2 = 1;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  BIQUAD_RET;
}

inline BiquadFilterDef designAudioEqAllpass(double omega, double q = 0.7071135624381276) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_CW0;
  BIQUAD_VAR_SW0;

  const double alpha = sw0 / (2 * q);
  const double b0 = 1 - alpha;
  const double b1 = -2 * cw0;
  const double b2 = 1 + alpha;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  BIQUAD_RET;
}

inline BiquadFilterDef designAudioEqPeaking(double omega, double bw, double dbgain) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_CW0;
  BIQUAD_VAR_SW0;

  const double a = pow(10, dbgain / 40);
  const double alpha = sw0 * sinh(std::log(2) / 2 * bw * omega / sw0);
  const double b0 = 1 + alpha * a;
  const double b1 = -2 * cw0;
  const double b2 = 1 - alpha * a;
  const double a0 = 1 + alpha / a;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha / a;
  BIQUAD_RET;
}

inline BiquadFilterDef designAudioEqLowShelf(double omega, double db_gain, double s = 1.0) {
  using namespace std;

  BIQUAD_VAR_W0;
  BIQUAD_VAR_CW0;
  BIQUAD_VAR_SW0;

  const double a = pow(10, db_gain / 40.0);

  const double beta = sqrt(a) * sqrt((a + 1 / a) * (1 / s - 1) + 2);

  const double b0 = a * ((a + 1) - (a - 1) * cw0 + beta * sw0);
  const double b1 = 2 * a * ((a - 1) - (a + 1) * cw0);
  const double b2 = a * ((a + 1) - (a - 1) * cw0 - beta * sw0);
  const double a0 = (a + 1) + (a - 1) * cw0 + beta * sw0;
  const double a1 = -2 * ((a - 1) + (a + 1) * cw0);
  const double a2 = (a + 1) + (a - 1) * cw0 - beta * sw0;
  BIQUAD_RET;
}

inline BiquadFilterDef designAudioEqHighShelf(double omega, double db_gain, double s = 1.0) {
  using namespace std;
  const double a = pow(10, db_gain / 40.0);
  BIQUAD_VAR_W0;
  BIQUAD_VAR_SW0;
  BIQUAD_VAR_CW0;
  const double beta = sqrt(a) * sqrt((a + 1 / a) * (1 / s - 1) + 2);

  const double b0 = a * ((a + 1) + (a - 1) * cw0 + beta * sw0);
  const double b1 = -2 * a * ((a - 1) + (a + 1) * cw0);
  const double b2 = a * ((a + 1) + (a - 1) * cw0 - beta * sw0);
  const double a0 = (a + 1) - (a - 1) * cw0 + beta * sw0;
  const double a1 = 2 * ((a - 1) - (a + 1) * cw0);
  const double a2 = (a + 1) - (a - 1) * cw0 - beta * sw0;
  BIQUAD_RET;
}

#undef BIQUAD_RET
#undef BIQUAD_VAR_W0
#undef BIQUAD_VAR_CW0
#undef BIQUAD_VAR_SW0

/*
 * A windowed sinc. Used primarily for upsampling/downsampling.
 *
 * Inspired by WDL's resampler.
 *
 * For the time being must be an odd length and
 * */
template <std::size_t N> auto designSincLowpass(double omega) -> IIRFilterDef<N, 1> {
  std::array<double, N> coefs{0.0};

  double center = (N - 1) / 2.0;

  for (std::size_t i = 0; i < N; i++) {
    double x = PI * (i - center);
    x *= omega * 2;

    double sinc = std::sin(x) / x;
    // blackman-harris.
    double y = (double)i / (N - 1);
    y *= 2 * PI;
    double window = 0.35875 - 0.48829 * cos(y) + 0.14128 * cos(2 * y) - 0.01168 * cos(3 * y);

    double out = i == center ? 1.0 : sinc * window;
    coefs[i] = out;
  }

  // Normalize DC to be a gain of 1.0.
  double gain = 0.0;
  for (unsigned int i = 0; i < coefs.size(); i++)
    gain += coefs[i];
  // add a little bit to the denominator to avoid divide by zero, at the cost of slight gain loss for small filters.
  gain = 1.0 / (gain + 0.01);

  IIRFilterDef<N, 1> ret;
  ret.gain = gain;
  for (unsigned int i = 0; i < ret.num_coefs.size(); i++)
    ret.num_coefs[i] = coefs[i];
  return ret;
}

} // namespace synthizer