#pragma once

#include "synthizer/cells.hpp"
#include "synthizer/config.hpp"
#include "synthizer/generation_thread.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

class WDL_Resampler;

namespace synthizer {

class AudioDecoder;
class Context;
class FadeDriver;

class StreamingGeneratorCommand {
	public:
	float *buffer;
	/* If set, seek before doing anything else. */
	std::optional<double> seek;
	/* Final position of the block. */
	double final_position = 0.0;
	/* Used to send events from the main thread. */
	unsigned int finished_count = 0, looped_count = 0;
	/**
	 * True if this block was partial, which can happen if the block
	 * had an error or was past the end of the stream.
	 * */
	bool partial = false;
};

class StreamingGenerator: public Generator {
	public:
	StreamingGenerator(const std::shared_ptr<Context> &ctx, const std::shared_ptr<AudioDecoder> &decoder);
	void initInAudioThread() override;
	~StreamingGenerator();

	int getObjectType() override;
	unsigned int getChannels() override;
	void generateBlock(float *output, FadeDriver *gain_driver) override;

	std::optional<double> startGeneratorLingering() override;

	#define PROPERTY_CLASS StreamingGenerator
	#define PROPERTY_LIST STREAMING_GENERATOR_PROPERTIES
	#define PROPERTY_BASE Generator
	#include "synthizer/property_impl.hpp"
	private:
	/* The body of the background thread. */
	void generateBlockInBackground(StreamingGeneratorCommand *command);

	GenerationThread<StreamingGeneratorCommand*> background_thread;
	std::shared_ptr<AudioDecoder> decoder = nullptr;
	std::shared_ptr<WDL_Resampler> resampler = nullptr;
	unsigned int channels = 0;
	/* Allocates commands contiguously. */
	deferred_vector<StreamingGeneratorCommand> commands;
	/* Allocates the buffers for the commands contiguously. */
	deferred_vector<float> buffer;
	double background_position = 0.0;
	/* Used to guard against spamming finished events. */
	bool sent_finished = false;
	/**
	 * Toggled in the audio thread when lingering begins.
	 * */
	bool is_lingering = false;
};

}
