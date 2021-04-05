#pragma once

#include "synthizer/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>

namespace synthizer {

/* Holds filter coefficients for an IIR filter. */
template<std::size_t num, std::size_t den, typename enabled = void>
class IIRFilterDef;

template<std::size_t num, std::size_t den>
class IIRFilterDef<num, den, typename std::enable_if<(num > 0)>::type> {
	public:
	/*
	 * Numerator of the filter (b_x in audio EQ cookbook).
	 * */
	std::array<double, num> num_coefs;
	/*
	 * Denominator of the filter (a_x in audio EQ cookbook).
	 * First coefficient is implicit and always 1.
	 * */
	std::array<double, den == 0 ? 0 : den-1> den_coefs;
	/*
	 * All filters are normalized so that a0 = 1.
	 * 
	 * This is the missing gain factor that needs to be added back in.
	 * */
	double gain;

	template<std::size_t NN, std::size_t ND, typename RES = typename std::enable_if_t<NN <= num && ND <= den, void>>
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

	template<std::size_t NN, std::size_t ND>
	bool operator==(const IIRFilterDef<NN, ND> &other) {
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

	template<std::size_t NN, std::size_t ND>
	bool operator!=(const IIRFilterDef<NN, ND> &other) {
		return !(*this == other);
	}
};

/*
 * A wire is the identity filter.
 * */
IIRFilterDef<1, 0> designWire();

/*
 * A single-zero filter.
 * Zero is on the x axis.
 * Normalized so that peak gain is 1.
 * */
IIRFilterDef<2, 1> designOneZero(double zero);

/*
 * A single-pole filter.
 * Normalized so that peak gain is 1.
 * */
IIRFilterDef<1, 2> designOnePole(double pole);

IIRFilterDef<2, 2> designDcBlocker(double r= 0.995);

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
BiquadFilterDef designAudioEqLowpass(double omega, double q = 0.7071135624381276);
BiquadFilterDef designAudioEqHighpass(double omega, double q = 0.7071135624381276);
/*
 * The peak gain of 0db variant.
 * 
 * In general filters try not to add energy on feedback loops.
 * */
BiquadFilterDef designAudioEqBandpass(double omega, double bw);
/* Aka band reject, but we use audio eq terminology. */
BiquadFilterDef designAudioEqNotch(double omega, double bw);
BiquadFilterDef designAudioEqAllpass(double omega, double q = 0.7071135624381276);
BiquadFilterDef designAudioEqPeaking(double omega, double bw, double dbgain);
BiquadFilterDef designAudioEqLowShelf(double omega, double db_gain, double s = 1.0);
BiquadFilterDef designAudioEqHighShelf(double omega, double db_gain, double s = 1.0);

/*
 * A windowed sinc. Used primarily for upsampling/downsampling.
 * 
 * Inspired by WDL's resampler.
 * 
 * For the time being must be an odd length and 
 * */
template<std::size_t N>
auto designSincLowpass(double omega)
-> IIRFilterDef<N, 1> {
	std::array<double, N> coefs{0.0};

	double center = (N - 1)/2.0;

	for(std::size_t i = 0; i < N; i++) {
		double x = PI*(i-center);
		x *= omega*2;

		double sinc = std::sin(x) / x;
		// blackman-harris.
		double y = (double) i / (N - 1);
		y *= 2 * PI;
		double window = 0.35875 - 0.48829 * cos(y) + 0.14128 * cos(2*y) - 0.01168 * cos(3*y);

		double out = i == center ? 1.0 : sinc*window;
		coefs[i] = out;
	}

	// Normalize DC to be a gain of 1.0.
	double gain = 0.0;
	for(unsigned int i = 0; i < coefs.size(); i++) gain += coefs[i];
	// add a little bit to the denominator to avoid divide by zero, at the cost of slight gain loss for small filters.
	gain = 1.0 / (gain + 0.01);

	IIRFilterDef<N, 1> ret;
	ret.gain = gain;
	for(unsigned int i = 0; i < ret.num_coefs.size(); i++) ret.num_coefs[i] = coefs[i];
	return ret;
}

template<std::size_t num1, std::size_t den1, std::size_t num2, std::size_t den2>
IIRFilterDef<num1 + num2 - 1, den1 + den2 - 1>
combineIIRFilters(IIRFilterDef<num1,  den1> f1, IIRFilterDef<num2, den2> f2) {
	double newGain = f1.gain * f2.gain;
	std::array<double, num1 + num2 - 1> workingNum;
	std::array<double, den1 + den2 - 1> workingDen;
	std::fill(workingNum.begin(), workingNum.end(), 0.0);
	std::fill(workingDen.begin(), workingDen.end(), 0.0);

	/*
	 * Convolve the numerators, then convolve the denominators.
	 * */
	for(unsigned int i = 0; i < num1; i++) {
		double num1r = f1.num_coefs[i];
		for(unsigned int j = 0; j < num2; j++) {
			double num2r = f2.num_coefs[j];
			workingNum[i + j] += num1r * num2r;
		}
	}

	for(unsigned int i = 0; i < den1; i++) {
		double den1r = i == 0 ? 1.0 : f1.den_coefs[i-1];
		for(unsigned int j = 0; j < den2; j++) {
			double den2r = j == 0 ? 1.0 : f2.den_coefs[j-1];
			workingDen[i + j] += den1r*den2r;
		}
	}

	IIRFilterDef<num1 + num2 - 1, den1 + den2 - 1> ret;
	ret.gain = newGain;
	std::copy(workingNum.begin(), workingNum.end(), ret.num_coefs.begin());
	std::copy(workingDen.begin() + 1, workingDen.end(), ret.den_coefs.begin());
	return ret;
}

}