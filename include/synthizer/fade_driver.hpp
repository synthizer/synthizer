#pragma once

#include "synthizer/config.hpp"
#include "synthizer/faders.hpp"

namespace synthizer {

/**
 * Abstracs fading values.
 * 
 * The common Synthizer pattern is to reconfigure a fader on every movement of a property of interest, then decide whether or not to run
 * a loop that does the fade or not by asking the faders if they're fading.  Using C++ templates, we can get this down to about 2 lines of code.
 * see source.cpp'sfillBlock  for a concrete example.
 * */
class FadeDriver {
	public:
	FadeDriver(float start_value, unsigned int fade_time_in_blocks): fade_time_in_blocks(fade_time_in_blocks), fader(start_value) {}

	void setValue(unsigned int time_in_blocks, float new_value) {
		this->fader = LinearFader{time_in_blocks, this->fader.getValue(time_in_blocks), time_in_blocks + this->fade_time_in_blocks, new_value};
	}

	/*
	 * This function takes a lambda which in turn takes a lambda that knows how to compute the gain, e.g.:
	 * driver.drive([](auto &g) { float gain = g(i); ... });
	 * Where i is the sample in the current block to compute the gain for.
	 * 
	 * This replaces code like:
	 * 
	 * if (fading) { //do fading }
	 * else { // do not fading }
	 * 
	 * where the actual bulk of the logic gets duplicated twice with minor differences.
	 * */
	template<typename CB>
	void drive(unsigned int time_in_blocks, CB &&callback) {
		if (this->fader.isFading(time_in_blocks)) {
			float start = this->fader.getValue(time_in_blocks);
			float end = this->fader.getValue(time_in_blocks + 1);
			float step = (end - start) / config::BLOCK_SIZE;
			auto val_comp = [start, step](unsigned int i) -> float {
				return start + step * i;
			};
			return callback(val_comp);
		} else {
			float val = this->fader.getValue(time_in_blocks);
			auto val_comp = [val](unsigned int i)  -> float {
				(void)i;
				return val;
			};
			return callback(val_comp);
		}
	}

	private:
	LinearFader fader;
	unsigned int fade_time_in_blocks;
};

}
