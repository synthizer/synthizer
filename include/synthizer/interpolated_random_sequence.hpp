#pragma once

#include "synthizer/random_generator.hpp"

#include <cassert>

namespace synthizer {

/*
 * An interpolated random sequence which will pull from the random number generator
 * on a specified period, produce random values in a specified range, and interpolate to the next one.
 * 
 * Like with faders, it's intended that users reconfigure these by reconstructing. To do so, tick the old one and feed that value to the new one
 * as the start value. To facilitate this use case, the first interpolation period will transition to within the range, and all others after that
 * will stay there.
 * */
class InterpolatedRandomSequence {
	public:
	InterpolatedRandomSequence(): InterpolatedRandomSequence(0.0f, 10, 0.0f, 1.0f) {}

	InterpolatedRandomSequence(float start_value, unsigned int steps, float min_value, float max_value) :
		range_min(min_value),
	 range_max(max_value),
	 steps_per_generation(steps),
	 steps_per_generation_inv(1.0f / steps),
	 last_value(start_value),
	 next_value(start_value) {
		 /*
		  * Force the first tick to return the start, then immediately recompute.
		  * */
		 this->countdown = 1;
		 this->range_size = (this->range_max - this->range_min);
		 this->step_size = this->next_value - this->last_value;
		 /*
		  *Prevent underflow.
		  */
		 if (this->steps_per_generation == 0) {
			 this->steps_per_generation = 1;
			 this->steps_per_generation_inv = 1.0f / this->steps_per_generation;
		 }
	 }

	float tick() {
		unsigned int since_last = this->steps_per_generation - this->countdown;
		float percent = since_last * this->steps_per_generation_inv;
		float val = this->last_value + percent * this->step_size;
		this->countdown--;
		if (this->countdown == 0) {
			this->last_value = this->next_value;
			float rand = this->generator.generateFloat();
			float mul = (1.0f + rand) * 0.5f;
			this->next_value = this->range_min + this->range_size * mul;
			this->countdown = this->steps_per_generation;
			this->step_size = this->next_value - this->last_value;
		}
		return val;
	}

	/*
	 * Returns the maximum value that this sequence can return. This is not just range_max because of
	 * transitions from initial values outside the range. Used in order to let us compute proper max delays.
	 * */
	float getMaxValue() {
		return std::max(this->last_value, this->range_max);
	}

	private:
	RandomGenerator generator;
	float range_min, range_max, range_size;
	unsigned int steps_per_generation;
	float steps_per_generation_inv;
	unsigned int countdown;
	float last_value, next_value, step_size;
};

}