/* Prints coefficients out of a FilterDef and magnitude/phase responses. */
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cmath>

#include "synthizer/config.hpp"
#include "synthizer/filter_design.hpp"
#include "synthizer/iir_filter.hpp"
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

static const complex<double> j{0, 1};

struct FrequencyResponseResult {
	double magnitude, phase, magnitude_db;
};

FrequencyResponseResult freqResultFromComplex(complex<double> in) {
	auto magnitude = abs(in);
	/* In degrees. */
	auto phase = arg(in)*PI/180;
	auto magnitude_db = 20*log10(magnitude);
	FrequencyResponseResult result;
	result.magnitude = magnitude;
	result.phase = phase;
	result.magnitude_db = magnitude_db;
	return result;
}

template<std::size_t num_s, std::size_t den_s>
FrequencyResponseResult computeFrequencyResponse(IIRFilterDef<num_s, den_s> &def, double  frequency) {
	double omega = frequency / synthizer::config::SR;
	// Prevents weirdness at DC.
	omega += 1e-9;
	double angle = 2 * PI*omega;
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

	auto fresponse = num/den;
	/*
		* This can happen.
		* */
	if (num.imag() == 0.0 && den.imag() == 0.0) {
		fresponse = {num.real()/den.real(), 0};
	}
	fresponse *= def.gain;
	return freqResultFromComplex(fresponse);
}

template<std::size_t num, std::size_t den>
FrequencyResponseResult verifyFrequency(IIRFilterDef<num, den> def, double frequency, unsigned int samples=synthizer::config::SR) {
	/* How far does the sinusoid advance on every sample? */
	double delta = frequency / config::SR;
	auto filter = makeIIRFilter<1>(def);
	complex<double> sum{0, 0};
	for (unsigned int i = 0; i < samples; i++) {
		float in, out;

		in = cos(2 * PI * delta * i);
		/* Filters add, in order to inline mixing, at least for now. SO we zero first. */
		out = 0.0f;
		filter.tick(&in, &out);

		auto complex_sinusoid = exp(2 * PI * j * (delta * i));
		sum += static_cast<double>(out) * complex_sinusoid;
	}
	sum /= static_cast<double>(samples) / 2;
	return freqResultFromComplex(sum);
}

void printFrequencyResult(FrequencyResponseResult res) {
	printf("Gain=%lfdb phase=%lf\n", res.magnitude_db, res.phase);
}

int main(int argc, char* argv[]) {
	auto def = designAudioEqHighShelf(3000.0 / config::SR, -3.0);
	printCoefs(def);
	printf("\n");
	while(true) {
		double freq;
		printf("> ");
		scanf("%lf", &freq);
		auto res = computeFrequencyResponse(def, freq);
		printFrequencyResult(res);
		double m1 = res.magnitude;
		printf("Verifying...\n");
		res = verifyFrequency(def, freq);
		double m2 = res.magnitude;
		printFrequencyResult(res);
		printf("Accuracy %2.2lf%%\n", (m1-(m1-m2))/m1*100.0);
	}
}
