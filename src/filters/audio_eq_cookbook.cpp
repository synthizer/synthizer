#include "synthizer/filter_design.hpp"
#include <cassert>
#include <cmath>

#include "synthizer/filter_design.hpp"
#include "synthizer/math.hpp"

namespace synthizer {

// Enough trig that this is justified.
using namespace std;

#define VAR_W0 const double w0 = 2 * PI * omega
#define VAR_CW0 const double cw0 = cos(w0)
#define VAR_SW0 const double sw0 = sin(w0)

static BiquadFilterDef makeRet(double b0, double b1, double b2, double a0, double a1, double a2) {
  BiquadFilterDef ret;
  const double gain = 1 / a0;
  a1 /= a0;
  a2 /= a0;

  ret.num_coefs = {b0, b1, b2};
  ret.den_coefs = {a1, a2};
  ret.gain = gain;
  return ret;
}

#define RET return makeRet(b0, b1, b2, a0, a1, a2);

BiquadFilterDef designAudioEqLowpass(double omega, double q) {
  VAR_W0;
  VAR_SW0;
  VAR_CW0;
  const double alpha = sw0 / (2 * q);

  const double b0 = (1 - cw0) / 2;
  const double b1 = 1 - cw0;
  const double b2 = b1 / 2;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  RET;
}

BiquadFilterDef designAudioEqHighpass(double omega, double q) {
  VAR_W0;
  VAR_CW0;
  VAR_SW0;
  const double alpha = sw0 / (2 * q);

  const double b0 = (1 + cw0) / 2;
  const double b1 = -(1 + cw0);
  const double b2 = (1 + cw0) / 2;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  RET;
}

BiquadFilterDef designAudioEqBandpass(double omega, double bw) {
  VAR_W0;
  VAR_SW0;
  VAR_CW0;

  const double alpha = sw0 * sinh(log(2) / 2 * bw * omega / sw0);
  const double b0 = alpha;
  const double b1 = 0;
  const double b2 = -alpha;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  RET;
}

BiquadFilterDef designAudioEqNotch(double omega, double bw) {
  VAR_W0;
  VAR_SW0;
  VAR_CW0;

  const double alpha = sw0 * sinh(log(2) / 2 * bw * omega / sw0);

  const double b0 = 1;
  const double b1 = -2 * cw0;
  const double b2 = 1;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  RET;
}

BiquadFilterDef designAudioEqAllpass(double omega, double q) {
  VAR_W0;
  VAR_CW0;
  VAR_SW0;
  const double alpha = sw0 / (2 * q);
  const double b0 = 1 - alpha;
  const double b1 = -2 * cw0;
  const double b2 = 1 + alpha;
  const double a0 = 1 + alpha;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha;
  RET;
}

BiquadFilterDef designAudioEqPeaking(double omega, double bw, double dbgain) {
  VAR_W0;
  VAR_CW0;
  VAR_SW0;

  const double a = pow(10, dbgain / 40);
  const double alpha = sw0 * sinh(log(2) / 2 * bw * omega / sw0);
  const double b0 = 1 + alpha * a;
  const double b1 = -2 * cw0;
  const double b2 = 1 - alpha * a;
  const double a0 = 1 + alpha / a;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha / a;
  RET;
}

BiquadFilterDef designAudioEqLowShelf(double omega, double db_gain, double s) {
  VAR_W0;
  VAR_CW0;
  VAR_SW0;

  const double a = pow(10, db_gain / 40.0);

  const double beta = sqrt(a) * sqrt((a + 1 / a) * (1 / s - 1) + 2);

  const double b0 = a * ((a + 1) - (a - 1) * cw0 + beta * sw0);
  const double b1 = 2 * a * ((a - 1) - (a + 1) * cw0);
  const double b2 = a * ((a + 1) - (a - 1) * cw0 - beta * sw0);
  const double a0 = (a + 1) + (a - 1) * cw0 + beta * sw0;
  const double a1 = -2 * ((a - 1) + (a + 1) * cw0);
  const double a2 = (a + 1) + (a - 1) * cw0 - beta * sw0;
  RET;
}

BiquadFilterDef designAudioEqHighShelf(double omega, double db_gain, double s) {
  const double a = pow(10, db_gain / 40.0);
  VAR_W0;
  VAR_SW0;
  VAR_CW0;
  const double beta = sqrt(a) * sqrt((a + 1 / a) * (1 / s - 1) + 2);

  const double b0 = a * ((a + 1) + (a - 1) * cw0 + beta * sw0);
  const double b1 = -2 * a * ((a - 1) + (a + 1) * cw0);
  const double b2 = a * ((a + 1) + (a - 1) * cw0 - beta * sw0);
  const double a0 = (a + 1) - (a - 1) * cw0 + beta * sw0;
  const double a1 = 2 * ((a - 1) - (a + 1) * cw0);
  const double a2 = (a + 1) - (a - 1) * cw0 - beta * sw0;
  RET;
}

} // namespace synthizer
