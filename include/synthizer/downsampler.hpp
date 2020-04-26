#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "synthizer/filter_design.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * A downsampler. Requires that the size be a power of 2 for the time being.
 * 
 * We use this for efficient delays, so that the delay lines themselves never have to deal with fractional values:
 * we can filter once at the end of processing instead.
 * 
 * For instance 44100 oversampled by a factor of 4 (for HRTF) gives delays of 2 microseconds,
 * 
 * The tick method consumes AMOUNT frames from the input, and produces 1 frame in the output.
 * */
template<std::size_t LANES, std::size_t AMOUNT, typename enabled = void>
class Downsampler;

template<std::size_t LANES, std::size_t AMOUNT>
class Downsampler<LANES, AMOUNT, All<void, PowerOfTwo<AMOUNT>, std::enable_if_t<AMOUNT != 1>>> {
	private:
	IIRFilter<LANES, 81, 3> filter;

	public:
	Downsampler() {
		// 0.6 is a rough approximation of a magic constant from WDL's resampler.
		// We want to pull the filter down so that frequencies just above nyquist don't alias.
		filter.setParameters(designSincLowpass<9>(1.0 / AMOUNT / 2));
	}

	void tick(float *in, float *out) {
		float tmp[LANES];
		filter.tick(in, out);
		for(int i = 1; i < AMOUNT; i++) {
			filter.tick(in+LANES*i, tmp);
		}
	}
};

}

