#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

/*
 * A generator: an abstraction over the concept of producing audio.
 * 
 * Examples of things that could be generators include noise, basic sine waves, and reading from streams.
 * 
 * Generators have two pieces of functionality:
 * 
 * - They output a block of samples, of up to config::MAX_CHANNELS channels (truncating if more).
 * - They adopt to pitch bends in a generator-defined fashion to participate in doppler for moving sources, and/or if asked by the user.
 * */
class AbstractGenerator {
	public:
	/* Return the number of channels this generator wants to output on the next block. */
	virtual unsigned int getChannels() = 0;
	/* Output a complete block of audio of config::BLOCK_SIZE. Is expected to add to the output, not replace. This buffer is always aligned. */
	virtual void generateBlock(AudioSample *output) = 0;
	/* Adapt to pitch bend, between 0.0 and any sufficiently large finite number. */
	virtual void setPitchBend(double newPitchBend) = 0;
};

}