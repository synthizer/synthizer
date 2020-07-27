#pragma once

#include "synthizer/boxes.hpp"
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
	void generateBlock(AudioSample *output) override;
	void setPitchBend(double newPitchBend) override;
	int getLooping();
	void setLooping(int looping);
	double getPosition();
	void setPosition(double newPosition);
	PROPERTY_METHODS;

	private:
	/* The body of the background thread. */
	void generateBlockInBackground(std::size_t channels, AudioSample *out);

	GenerationThread background_thread;
	std::shared_ptr<AudioDecoder> decoder = nullptr;
	std::shared_ptr<WDL_Resampler> resampler = nullptr;
	unsigned int channels = 0;
	std::atomic<int> looping = 0;
	std::atomic<double> position = 0.0;
	FiniteDoubleBox next_position = 0.0;

	double pitch_bend = 1.0;
	/*
	 * For pitch bend, the fractional offset that the last iteration used.
	 * */
	float fractional_offset = 0.0f;
};

}