#pragma once

#include <algorithm>
#include <cstddef>
#include <tuple>

#include "synthizer/config.hpp"
#include "synthizer/block_delay_line.hpp"
#include "synthizer/data/hrtf.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * HRTF implementation.
 * 
 * This involves a variety of components which interact with one another to provide efficient HRTF processing of audio data:
 * 
 * - Low-level functions, which compute HRIRs and interaural time delay.
 * - A higher level piece, the HrtfPannerBlock, which does HRTF on 4 sources at once.
 * - A yet higher level piece, the HrtfPannerBank, which will eventually implement the as yet undefined panner bank interface.
 * 
 * NOTE: because of established conventions for HRIR data that we didn't set, angles here are in degrees in order to match our data sources.
 * Specifically azimuth is clockwise of forward and elevation ranges from -90 to 90.
 * 
 * Also the dataset is asumed to be for the left ear. Getting this backward will cause the impulses to be flipped.
 * */


/*
 * Parameters for an Hrir model.
 * 
 * All units are SI.
 * */
struct HrirParameters {
	/* head_radius is a guess. TODO: figure out a better way to compute this number. */
	double head_radius = 0.08;
	double speed_of_sound = 343;
};

/*
 * Compute the interaural time difference for a source in oversampled samples.
 * 
 * Returns a tuple {l, r} where one of l or r is always 0.
 * 
 * Unit is fractional samples.
 * */
std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation);
std::tuple<double, double> computeInterauralTimeDifference(double azimuth, double elevation, const HrirParameters *hrir_parameters);

/*
 * Read the built-in HRIR dataset, interpolate as necessary, and fill the output buffers with impulses for convolution.
 * */
void computeHrtfImpulses(double azimuth, double elevation, float *left, unsigned int left_stride, float *right, unsigned int right_stride);

/*
 * A bank of 4 parallel HRTF convolvers with ITD.
 * 
 * We don't template this because anything bigger than 4 is likely as not to cause problems, and it's convenient to not have all of this code inline. That said, note the class-level consts: this is intentionally written so that it can be templatized later.
 * */
class HrtfConvolver {
	public:
	static const std::size_t CHANNELS = 4;

	/*
	 * Returns a tuple of (pointer, stride).
	 * */
	std::tuple<AudioSample *, std::size_t> getInputBuffer(unsigned int channel);

	/*
	 * Parameters:
	 * output: buffer to write to. We add, so zero it first.
	 * azimuths: 4 floats, the azimuths for the channels.
	 * elevations: the elevations for the channels.
	 * moved: An array of 4 bools, whether each of these sources moved. Should be false for the first update.
	 * 
	 * */
	void computeOutput(const std::array<double, 4> &azimuths,
		const std::array<double, 4> &elevations,
		const std::array<bool, 4> &moved,
		AudioSample *output);

	/* Used when recycling a channel for another source. Sets that channel to 0. */
	void clearChannel(unsigned int channel);

	private:
	template<typename R>
	void stepConvolution(R &&reader, const float *hrir, AudioSample4 *dest_l, AudioSample4 *dest_r);

	/*
	 * 2 blocks plus the max ITD.
	 * We'll use modulus on 1/3 blocks on average.
	 * */
	BlockDelayLine<CHANNELS, nextMultipleOf(config::HRTF_MAX_ITD + config::BLOCK_SIZE * 2, config::BLOCK_SIZE)/ config::BLOCK_SIZE> input_line;
	/* This is an extra sample lon to make linear interpolation easier. */
	BlockDelayLine<CHANNELS * 2, nextMultipleOf(config::BLOCK_SIZE*2 + config::HRTF_MAX_ITD + 1, config::BLOCK_SIZE) / config::BLOCK_SIZE> itd_line;

	/*
	 * The hrirs and an index which determines which we are using.
	 * 
	 * The way this works is that current_hrir is easily flipped with xor. current_hrir ^ 1 is the previous, (current_hrir ^ 1)*CHANNELS*2*hrtf_data::RESPONSE_LENGTH is the index of the previous.
	 * 
	 * These are stored like: [l1, l2, l3, l4, r1, r2, r3, r4]
	 * We do the convolution with a specialized kernel that can deal with this.
	 * */
	static_assert(hrtf_data::IMPULSE_LENGTH % (config::ALIGNMENT / sizeof(AudioSample)) == 0, "Hrtf dataset length must be a multiple of  alignment/sizeof(AudioSample) for SIMD purposes");
	alignas(config::ALIGNMENT) std::array<AudioSample, hrtf_data::IMPULSE_LENGTH*CHANNELS*2*2> hrirs = { 0.0f };
	unsigned int current_hrir = 0;
	std::array<std::tuple<double, double>, CHANNELS> prev_itds{};
};

}
