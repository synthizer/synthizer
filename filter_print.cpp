/* Prints coefficients out of a FilterDef and magnitude/phase responses. */
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cmath>

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

template<std::size_t num, std::size_t den>
void printFrequencyResponse(IIRFilterDef<num, den> &def, int start = 0, int stop = 20, int bins = 20) {
	printf("Omega magnitude(db) phase(degrees)\n");
	for(int i = start; i < stop; i++) {
		double omega = i / (double)bins;
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

//	printf("%f %f | %f %f\n", num.real(), num.imag(), den.real(), den.imag());

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
		auto magnitudeDb = 20*log10(magnitude);
		printf("%.3f, %.3f, %.3f\n", omega, magnitudeDb, phase);
	}
}

int main(int argc, char* argv[]) {
	auto def = designSincLowpass<31>(0.25);
	//auto def = designAudioEqLowpass(0.25);
//	auto def = designDcBlocker();
	printCoefs(def);
	printf("\n");
	printFrequencyResponse(def, 0, 20, 20);
	return 0;
}
