#include <array>
#include <complex>
#include <cmath>
#include <tuple>

#include "synthizer/filter_design.hpp"

using namespace std;

namespace synthizer {

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
	ret.gain = 1;
	ret.den_coefs = {};
	return ret;
}

IIRFilterDef<1, 2> designOnePole(double pole) {
	double a0, a1;
	// poles are zero in the denominator.
	std::tie(a0, a1) = coefsForZero(pole);
	IIRFilterDef<1, 2> ret;
	ret.num_coefs = {};
	ret.den_coefs = {a1};
	ret.gain = 1;
	return ret;
}

IIRFilterDef<2, 2> designDcBlocker(double r) {
	auto num = designOneZero(1);
	auto den = designOnePole(r);
	return combineIIRFilters(num, den);
}

}