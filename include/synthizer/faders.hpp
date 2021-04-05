#pragma once

#include <cassert>

namespace synthizer  {

/*
 * Various kinds of fader.
 * 
 * These work in block time. External APIs convert for efficiency.
 * 
 * Faders guarantee that they will stop fading by a specific block, and that
 * once they've done so, they'll return the exact output value specified.  This is used by the router to avoid having to maintain an FSM
 * 
 * Faders are configured once at construction, then reconfigured by reconstructing as necessary. This is done because Synthizer
 * needs to be well behaved in the presence of rapid user changes.
 * */

/*
 * A linear fader: slope * x + start.
 * */
class LinearFader {
	public:
	LinearFader(unsigned int start_time, float start_value, unsigned int end_time, float end_value): start_time(start_time), end_time(end_time), start_value(start_value), end_value(end_value) {
		assert(end_time >= start_time);
		/**
		 * Lots of places make faders that don't fade by using start_time = end_time = 0, which causes MSVC
		 * warning C4723, due to a genertaed divide by zero. Branch, detecting this case,
		 * to make sure that MSVC doesn't complain.
		 * */
		if (start_time == end_time) {
			this->slope = 1.0f;
		} else {
			this->slope = (end_value - start_value) / (end_time - start_time);
		}
	}

	/*
	 * Convenience constructor for a fader fixed at a specific value.
	 * */
	LinearFader(float value): LinearFader(0, value, 0, value) {}

	float getValue(unsigned int block_time) {
		assert(block_time >= this->start_time);
		return block_time >= end_time ? this->end_value : this->start_value + slope * (block_time - start_time);
	}

	float getFinalValue() {
		return this->end_value;
	}

	bool isFading(unsigned int block_time) {
		assert(block_time >= this->start_time);
		return block_time < this->end_time;
	}

	private:
	unsigned int start_time, end_time;
	float start_value, slope, end_value;
};

}