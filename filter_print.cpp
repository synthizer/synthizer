/* Prints coefficients out of a FilterDef and magnitude/phase responses. */
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cmath>

#include "synthizer/config.hpp"
#include "synthizer/filter_design.hpp"
#include "synthizer/math.hpp"

using namespace synthizer;
using namespace std;

template<std::size_t num, std::size_t den>
void printCoefs(IIRFilterDef<num, den> &def) {
	printf("gain: %.3f\n", def.gain);
	printf("Numerator:");
	for (auto i: def.num_coefs) {
		printf(" %.3f ", i);
	}
	printf("\ndenominator: 1.0");
	for(auto i: def.den_coefs) {
		printf(" %.3f", i);
	}
	printf("\n");
}

struct FrequencyResponseResult {
	double magnitude, phase, magnitude_db;
};

template<std::size_t num, std::size_t den>
FrequencyResponseResult computeFrequencyResponse(IIRFilterDef<num, den> &def, double  frequency) {
	double omega = 0.5 * frequency / synthizer::config::SR;
	// Prevents weirdness at DC.
	omega += 1e-9;
	double angle = 2 * PI*omega;
	complex<double> j{0, 1};
	complex<double> input = exp(j*angle);

	complex<double> num{0, 0};
	for(int i = 0; i < def.num_coefs.size(); i++) {
		double power = -(i);
		double coef = def.num_coefs[i];
		auto term = coef*pow(input, power);
		num += term;
	}

	complex<double> den{1, 0};
	for(int i = 0; i < def.den_coefs.size(); i++) {
		double power  = -(i+1);
		double coef = def.den_coefs[i];
		auto term = coef*pow(input, power);
		den += term;
	}

	auto fresponse = def.gain*(num/den);
	/*
		* This can happen.
		* */
	if (num.imag() == 0.0 && den.imag() == 0.0) {
		fresponse = {num.real()/den.real(), 0};
	}

	auto magnitude = abs(fresponse);
	/* In degrees. */
	auto phase = arg(fresponse)*PI/180;
	auto magnitude_db = 20*log10(magnitude);
	FrequencyResponseResult result;
	result.magnitude = magnitude;
	result.phase = phase;
	result.magnitude_db = magnitude_db;
	return result;
}

int main(int argc, char* argv[]) {
	auto def = combineIIRFilters(designOneZero(-1), designOnePole(0.9929146));
	printCoefs(def);
	printf("\n");
	while(true) {
		double freq;
		printf("> ");
		scanf("%lf", &freq);
		auto res = computeFrequencyResponse(def, freq);
		printf("Gain=%lfdb phase=%lf\n", res.magnitude_db, res.phase);
	}
}
