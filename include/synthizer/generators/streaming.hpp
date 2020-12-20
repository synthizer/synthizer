#pragma once

#include "synthizer/cells.hpp"
#include "synthizer/config.hpp"
#include "synthizer/generation_thread.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include <atomic>
#include <memory>

class WDL_Resampler;

namespace synthizer {

class AudioDecoder;
class Context;

class StreamingGenerator: public Generator {
	public:
	StreamingGenerator(const std::shared_ptr<Context> &ctx, const std::shared_ptr<AudioDecoder> &decoder);
	~StreamingGenerator();

	unsigned int getChannels() override;
	void generateBlock(float *output) override;
	#define PROPERTY_CLASS StreamingGenerator
	#define PROPERTY_LIST STREAMING_GENERATOR_PROPERTIES
	#define PROPERTY_BASE Generator
	#include "synthizer/property_impl.hpp"
	private:
	/* The body of the background thread. */
	void generateBlockInBackground(std::size_t channels, float *out);

	GenerationThread background_thread;
	std::shared_ptr<AudioDecoder> decoder = nullptr;
	std::shared_ptr<WDL_Resampler> resampler = nullptr;
	unsigned int channels = 0;
	std::atomic<double> background_position = 0.0;
	FiniteDoubleCell next_position = 0.0;

	/*
	 * For pitch bend, the fractional offset that the last iteration used.
	 * */
	float fractional_offset = 0.0f;
};

}
