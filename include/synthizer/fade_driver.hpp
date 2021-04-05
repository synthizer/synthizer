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
 * 
 * In order to support setting values before an object is first audible, the FadeDriver will ignore crossfades which are
 * due to value changes before the first time it ticks.  If an object is silent, it is possible to restore this behavior by calling outputIsSilent, so that value changes
 * will again not crossfade until the next time it's ticked.
 * */
class FadeDriver {
	public:
	FadeDriver(float start_value, unsigned int fade_time_in_blocks): fader(start_value), fade_time_in_blocks(fade_time_in_blocks) {}

	/**
	 * Will update instantaneously if this driver is aware that the output is silent.
	 * 
	 * If fade_time_in_blocks is 0, uses the default set with the constructor. This is the default. Otherwise,
	 * use the value provided in the function.
	 * */
	void setValue(unsigned int block_time, float new_value, unsigned int fade_time = 0) {
		unsigned int ft = fade_time != 0 ? fade_time : this->fade_time_in_blocks;

		if (this->was_silent) {
			this->fader = LinearFader(new_value);
		} else {
			this->fader = LinearFader{block_time, this->fader.getValue(block_time), block_time + ft, new_value};
		}
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
		this->was_silent = false;
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

	/**
	 * Returns true if the block at the specified time is either
	 * going to have a value > that specified or if said block is crossfading.
	 * 
	 * This is useful primarily for things like the effect routing infrastructure and the
	 * autodelete stuff, which wants to do something special after fadeouts.
	 * */
	bool isActiveAtTime(unsigned int time_in_blocks, float threshold = 0.0f) {
		return this->fader.isFading(time_in_blocks) || this->fader.getValue(time_in_blocks) > threshold ||this->fader.getValue(time_in_blocks + 1) > threshold;
	}

	/**
	 * Inform the fader that output is silent, and we once again desire instantaneous changes.
	 * 
	 * Will also update the current value to not crossfade if necessary.
	 * */
	void outputIsSilent() {
		this->was_silent = true;
		this->fader = LinearFader(this->fader.getFinalValue());
	}

	private:
	LinearFader fader;
	unsigned int fade_time_in_blocks;
	bool was_silent = true;
};

}
