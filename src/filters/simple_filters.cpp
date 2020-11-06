#include <array>
#include <complex>
#include <cmath>
#include <tuple>

#include "synthizer/filter_design.hpp"

using namespace std;

namespace synthizer {

IIRFilterDef<1, 0> designWire() {
	IIRFilterDef<1, 0> def;
	def.gain = 1.0;
	def.num_coefs[0] = 1.0;
	return def;
}

/*
 * Compute coefficients for a zero of the polynomial a0+a1 z^-1
 * 
 * a1 = -x because a0 =1  in all cases, so
 * 1 + a1 z^-1 = 0
 * 1 = -a1 z^-1
 * 1 = -a1 / z
 * z = -a1
 * */
tuple<double, double> coefsForZero(double x) {
	return {1, -x};
}

IIRFilterDef<2, 1> designOneZero(double zero) {
	double b0, b1;
	std::tie(b0, b1) = coefsForZero(zero);
	IIRFilterDef<2, 1> ret;
	ret.num_coefs = {b0, b1};
	ret.gain = 1.0 / (abs(b0) + abs(b1));
	ret.den_coefs = {};
	return ret;
}

IIRFilterDef<1, 2> designOnePole(double pole) {
	double a0, a1;
	// poles are zero in the denominator.
	std::tie(a0, a1) = coefsForZero(pole);
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

IIRFilterDef<2, 2> designDcBlocker(double r) {
	auto num = designOneZero(1);
	auto den = designOnePole(r);
	return combineIIRFilters(num, den);
}

}