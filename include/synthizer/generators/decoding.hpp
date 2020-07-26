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

/*
 * A decoding generator generates audio from a specific decoder, which can be changed at runtime.
 * 
 * This works over decoders, not streams, because that lets us enforce that whatever audio the user wanted to decode was properly decodable before it gets as far as the audio thread.
 * */
class DecodingGenerator: public Generator {
	public:
	DecodingGenerator(const std::shared_ptr<Context> &ctx, const std::shared_ptr<AudioDecoder> &decoder);
	~DecodingGenerator();

	unsigned int getChannels();
	void generateBlock(AudioSample *output);
	void setPitchBend(double newPitchBend);
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