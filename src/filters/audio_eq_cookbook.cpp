#include "synthizer/filter_design.hpp"
#include <cassert>
#include <cmath>

#include "synthizer/filter_design.hpp"
#include "synthizer/math.hpp"

#pragma GCC diagnostic ignored "-Wunused-variable"

namespace synthizer {

// Enough trig that this is justified.
using namespace std;

#define VAR_A const double a = pow(10, dbgain / 40)
#define VAR_W0 const double w0 = 2 * PI * omega
#define VAR_CW0 const double cw0 = cos(w0)
#define VAR_SW0 const double sw0 = sin(w0)
#define VAR_ALPHA const double alpha = sw0 / (2 * q)
#define VAR_SHELFIM const double shelfim = 2 * sqrt(a) * alpha

#define VAR_Q_S                                                                                                        \
  const double q_r = sqrt((a + 1 / a) * (1 / s - 1) + 2);                                                              \
  const double q = 1 / q_r

#define VAR_Q_BW                                                                                                       \
  const double q_r = 2 * sinh(log(2) / 2 * bw * w0 * sin(w0));                                                         \
  const double q = 1 / q_r

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
  VAR_ALPHA;
  VAR_CW0;
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
  VAR_ALPHA;
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
  VAR_Q_BW;
  VAR_ALPHA;
  VAR_CW0;
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
  VAR_Q_BW;
  VAR_ALPHA;
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
  VAR_ALPHA;
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
  VAR_Q_BW;
  VAR_ALPHA;
  VAR_A;
  const double b0 = 1 + alpha * a;
  const double b1 = -2 * cw0;
  const double b2 = 1 - alpha * a;
  const double a0 = 1 + alpha / a;
  const double a1 = -2 * cw0;
  const double a2 = 1 - alpha / a;
  RET;
}

BiquadFilterDef designAudioEqLowShelf(double omega, double db_gain, double s) {
  /* Note: these are actually different from all the others. Not using the macros isn't a mistake. */
  const double a = pow(10, db_gain / 40.0);
  const double w0 = 2 * PI * omega;
  const double cw0 = cos(w0);
  const double sw0 = sin(w0);
  const double alpha = sqrt((a * a + 1) / s - (a - 1) * (a - 1));

  const double b0 = a * ((a + 1) - (a - 1) * cw0 + alpha * sw0);
  const double b1 = 2 * a * ((a - 1) - (a + 1) * cw0);
  const double b2 = a * ((a + 1) - (a - 1) * cw0 - alpha * sw0);
  const double a0 = (a + 1) + (a - 1) * cw0 + alpha * sw0;
  const double a1 = -2 * ((a - 1) + (a + 1) * cw0);
  const double a2 = (a + 1) + (a - 1) * cw0 - alpha * sw0;
  RET;
}

BiquadFilterDef designAudioEqHighShelf(double omega, double db_gain, double s) {
  /* Note: these are actually different from all the others. Not using the macros isn't a mistake. */
  const double a = pow(10, db_gain / 40.0);
  VAR_W0;
  VAR_SW0;
  VAR_CW0;
  VAR_Q_S;
  VAR_ALPHA;
  VAR_SHELFIM;
  const double b0 = a * ((a + 1) + (a - 1) * cw0 + shelfim);
  const double b1 = -2 * a * ((a - 1) + (a + 1) * cw0);
  const double b2 = a * ((a + 1) + (a - 1) * cw0 - shelfim);
  const double a0 = (a + 1) - (a - 1) * cw0 + shelfim;
  const double a1 = 2 * ((a - 1) - (a + 1) * cw0);
  const double a2 = (a + 1) - (a - 1) * cw0 - shelfim;
  RET;
}

} // namespace synthizer
