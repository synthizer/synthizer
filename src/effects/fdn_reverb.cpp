#include "synthizer/effects/fdn_reverb.hpp"

#include "synthizer_properties.h"

#include "synthizer/c_api.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/math.hpp"
#include "synthizer/prime_helpers.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

namespace synthizer {

void FdnReverbEffect::recomputeModel() {
	/*
	 * Design the input filter. It's as straightforward as it looks.
	 * */
	if (this->input_filter_enabled) {
		this->input_filter.setParameters(designAudioEqLowpass(this->input_filter_cutoff / config::SR));
	} else {
		this->input_filter.setParameters(designWire());
	}


	this->late_reflections_delay_samples = this->late_reflections_delay * config::SR;

	/*
	 * The average delay line length is the mean free path. You can think about this as if delay lines in the FDN all ran to walls.
	 * We want the average distance to those walls to be the mean free path.
	 * 
	 * We also want the delay lines to not be tuned, which effectively means coprime. The easiest way to do that is to use
	 * a set of prime numbers. Other schemes are possible, for example primes raised to powers, but some Python prototyping shows
	 * that this has reasonable error, and it's easy enough to just embed an array of primes to pull from.
	 * 
	 * Finally, if we just choose all of the lines to have the mean free path as a prime number (or something close to it) it'll take a long time
	 * for it to stop sounding like an echo.  We solve this by choosing some of our delay lines to be much shorter, and some to be much longer. How much this spread occurs is controlled by the late reflection diffusion.
	 * 
	 * The math works like this:
	 * 
	 * The first 2 are at the exact point of the mean free path. It would be more mathematically consistent for this just to be 1, but keeping the line count
	 * at a multiple of 4 is very good for efficiency.
	 * 
	 * The next 2 after that are at some multiples of the mean free path. At maximum diffusion, they'd be 0.5 and 1.5
	 * The next 2 would be 0.25 and 1.75.
	 * And so on. SO that we have:
	 * (1m + 1m) + (0.5m + 1.5m) + ... = 2 + 2 + ... = 2*(n/2)*m = n*m
	 * where n is the line count and m the mean free path in samples. There is obviously error here because our array is an array of primes, but it's a reasonable approximation of the reality we'd like to have.
	 * 
	 * We vary the diffusion by changing the denominator of the fraction in the above, and expose a percent-based control for that purpose.
	 * Using significantly large numbers (on the order of 2) for the exponent makes strange things happen for large diffusions; I tuned the constants in the below
	 * difffusion equation by ear.
	 * */
	unsigned int mean_free_path_samples = this->mean_free_path * config::SR;
	this->delays[0] = getClosestPrime(mean_free_path_samples);
	this->delays[1] = getClosestPrimeRestricted(mean_free_path_samples, 1, &this->delays[0]);

	float diffusion_base = 1.0 + 0.4 * this->late_reflections_diffusion;
	for (unsigned int i = 2; i < LINES; i+=2) {
		unsigned int iteration = i/2 + 1;
		float fraction = 1.0f / powf(diffusion_base, iteration);
		delays[i] = getClosestPrimeRestricted(mean_free_path_samples * fraction, i, &this->delays[0]);
		delays[i+1] = getClosestPrimeRestricted(mean_free_path_samples * (2.0f - fraction), i+1, &delays[0]);
	}

	/*
	 * If the user gave us an extreme value for mean fre path, it's possible that we are going over the maximum we can handle. While
	 * this  would normally be guarded by the property range mechanisms, reading from the prime array can return values over the maximum.
	 * */
	for (unsigned int i = 0; i < LINES; i++) {
		this->delays[i] = std::min(this->delays[i], MAX_FEEDBACK_DELAY);
	}

	/*
	 * In order to minimize clipping and/or other crossfading artifacts, keep the lines sorted from least
	 * delay to greatest.
	 * */
	std::sort(this->delays.begin(), this->delays.end());

	/*
	 * The gain for each line is computed from t60, the time to decay to -60 DB.
	 * 
	 * The math to prove why this works is involved, see: https://ccrma.stanford.edu/~jos/pasp/Achieving_Desired_Reverberation_Times.html
	 * What we do works for any matrix, but an intuitive (but inaccurate) explanation of our specific case follows:
	 * 
	 * Imagine that samples traveling through the FDN are balls on a track, choosing some random path. What matters for the decay curve is how fast the ball slows down,
	 * i.e. the friction in this analogy.  If the ball has traveled 1 second and our t60 is 1 second, we want it to have "slowed down" by 60db.
	 * This implies that the track has uniform friction.  Each line has delay samples of track, so we can work out the gain by determining what it should be
	 * per sample, and multiplying by the length of the track.
	 * 
	 * We control this with an equalizer, which lets the user control how "bright" the reverb is.
	 * */
	float decay_per_sample_db = -60.0f / this->t60 / config::SR;
	for (unsigned int i = 0; i < LINES; i++) {
		unsigned int length = this->delays[i];
		float decay_db = length * decay_per_sample_db;
		ThreeBandEqParams params;
		params.dbgain_lower = decay_db * this->late_reflections_lf_rolloff;
		params.freq_lower = this->late_reflections_lf_reference;
		params.dbgain_mid = decay_db;
		params.dbgain_upper = decay_db * this->late_reflections_hf_rolloff;
		params.freq_upper = this->late_reflections_hf_reference;
		this->feedback_eq.setParametersForLane(i, params);
	}

	/*
	 * The modulation depth and rate.
	 * */
	unsigned int mod_depth_in_samples = this->late_reflections_modulation_depth * config::SR;
	unsigned int mod_rate_in_samples = config::SR / this->late_reflections_modulation_frequency;
	for (unsigned int i = 0; i < LINES; i++) {
		/*
		 * vary the frequency slightly so that they're not all switching exactly on the same sample.
		 * This smoothes out the peak of what is effectively a triangle wave.
		 * */
		this->late_modulators[i] = InterpolatedRandomSequence(this->late_modulators[i].tick(), mod_rate_in_samples + i, 0.0f, mod_depth_in_samples);
	}
	/*
	 * account for the extra sample we need when interpolating here.
	 * */
	this->max_delay = this->delays[LINES - 1] + mod_depth_in_samples + 1;
	this->max_delay = std::max(this->max_delay, this->late_reflections_delay_samples);
}

void FdnReverbEffect::resetEffect() {
	this->lines.clear();
	this->feedback_eq.reset();
	this->input_filter.reset();
}

void FdnReverbEffect::runEffect(unsigned int time_in_blocks, unsigned int input_channels, float *input, unsigned int output_channels, float *output, float gain) {
	/*
	 * Output is stereo. For surround setups, we can get 99% of the benefit just by upmixing stereo differently, which we'll do in future by hand
	 * as a special case.
	 * */
	alignas(config::ALIGNMENT) thread_local std::array<float, config::BLOCK_SIZE * 2> output_buf{ { 0.0f } };
	float *output_buf_ptr = &output_buf[0];

	if (this->dirty) {
		this->recomputeModel();
		this->dirty = false;
	}

	/*
	 * Normally we would go through a temporary buffer to premix channels here, and we still might, but we're using the delay line's write loop and it's always to mono; just do it inline.
	 * */
	this->lines.runRwLoop(this->max_delay, [&](unsigned int i, auto &rw) {
		float *input_ptr = input + input_channels * i;
		float input_sample = 0.0f;

		/* Mono downmix. */
		for (unsigned int ch = 0; ch < input_channels; ch++) {
			input_sample += input_ptr[ch];
		}
		input_sample *= 1.0f / input_channels;

		/*
		 * Apply the initial filter. The purpose is to let the user make the reverb less harsh on high-frequency sounds.
		 * */
		this->input_filter.tick(&input_sample, &input_sample);

		/*
		* Implement a householder reflection about <1, 1, 1, 1... >. This is a reflection about the hyperplane.
		* */

		/* not initialized because zeroing can be expensive and we set it immediately in the loop below. */
		std::array<float, LINES> values;
		for (unsigned int i = 0; i < LINES; i++) {
			double delay = this->delays[i] + this->late_modulators[i].tick();
			float w2 = delay - std::floor(delay);
			float w1 = 1.0f - w2;
			float v1 = rw.read(i, delay);
			float v2 = rw.read(i, delay);
		 values[i] = v1 * w1 + v2 * w2;
		}

		/*
	 	* Pass it through the equalizer, which handles feedback.
	 	* */
		this->feedback_eq.tick(&values[0], &values[0]);

		float sum = std::accumulate(values.begin(), values.end(), 0.0f);
		sum *= 2.0f / LINES;

		/*
		 * Write it back.
		 * */
		float input_per_line = input_sample * (1.0f / LINES);
		for (unsigned int	 i = 0; i < LINES; i++) {
			rw.write(i, values[i] - sum + input_per_line);
		}

		/*
		 * We want two decorrelated channels, with roughly the same amount of reflections in each.
		 * */
		unsigned int d = this->late_reflections_delay_samples;
		float *frame = rw.readFrame(d);
		float l = frame[0] + frame[2] + frame[4] + frame[6];
		float r = frame[1] + frame[3] + frame[5] + frame[7];
		output_buf_ptr[2*i] = l * gain;
		output_buf_ptr[2*i + 1] = r * gain;
	});

	mixChannels(config::BLOCK_SIZE, output_buf_ptr, 2, output, output_channels);
}

}

#define PROPERTY_CLASS GlobalFdnReverbEffect
#define PROPERTY_LIST FDN_REVERB_EFFECT_PROPERTIES
#define PROPERTY_BASE BaseObject
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<GlobalFdnReverbEffect>(1);
	*out = toC(x);
	std::shared_ptr<GlobalEffectBase> e = x;
	ctx->registerGlobalEffect(	e);
	return 0;
	SYZ_EPILOGUE
}
