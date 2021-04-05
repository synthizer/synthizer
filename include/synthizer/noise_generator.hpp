#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/iir_filter.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/types.hpp"


#include <array>
#include <cstdint>

namespace synthizer {

class NoiseGenerator {
	public:
	NoiseGenerator();

	int getNoiseType();
	void setNoiseType(int type);
	float generateSample();
	/* Adds, as with the rest of Synthizer. */
	void generateBlock(unsigned int size, float *block, unsigned int stride = 1);

	private:
	float generateSampleUniform();
	void generateBlockUniform(unsigned int size, float *block, unsigned int stride);

	RandomGenerator random_gen;
	int noise_type = SYZ_NOISE_TYPE_UNIFORM;

	/*
	 * Voss-mcCartney algorithm to generate pink noise.
	 * 
	 * This consists of some number of stacked random generators, which are set up so that we only update some at a time. I.e.
	 * sample 1: 0
	 * sample 2: 1
	 * sample 3: 0
	 * sample 4: 2
	 * sample 5: 0
	 * sample 6: 1
	 * 
	 * And so on. Notice that 0 is updated twice as often as 1,
	 * 1 twice as often as 2, and so on.  This effectively gives us decreasing power in the higher octaves, though
	 * this algorithm was taken from other people and I don't have a proof that it works to generate pink noise reliably.
	 * */
	/* Must never be greater than 32. */
	static const unsigned int VM_GENERATOR_COUNT = 14;
	/*
	 * One generator is always run, so we don't do it here.
	 * */
	std::array<float, VM_GENERATOR_COUNT - 1> vm_values = {0.0f};
	float vm_accumulator = 0.0f;
	/*
	 * Mask to turn the counter into something safe for __builtin_ctz. By oring with the counter,
	 * we make sure the high bits are always set, that ctz never gets 0, and that ctz never returns something
	 * higher than the maximum array index.
	 * 
	 * -2 because this accounts for the always-run generator.
	 * */
	static const unsigned int VM_MASK = ~((1 << (VM_GENERATOR_COUNT - 2u)) - 1u);
	unsigned int vm_counter = 0;
	void initVM();
	float generateSampleVM();
	void generateBlockVM(unsigned int size, float *block, unsigned int stride);

	/* -6DB filter for brown noise. */
	IIRFilter<1, 3, 3> brown_filter;
	void initFilteredBrown();
	float generateSampleFilteredBrown();
	void generateBlockFilteredBrown(unsigned int size, float *block, unsigned int stride);
};

}