#pragma once

#include "synthizer/abstract_generator.hpp"
#include "synthizer/config.hpp"
#include "synthizer/types.hpp"

#include <memory>

class WDL_Resampler;

namespace synthizer {

class AudioDecoder;

/*
 * A decoding generator generates audio from a specific decoder, which can be changed at runtime.
 * 
 * This works over decoders, not streams, because that lets us enforce that whatever audio the user wanted to decode was properly decodable before it gets as far as the audio thread.
 * */
class DecodingGenerator: public AbstractGenerator {
	public:
	DecodingGenerator() {}
	~DecodingGenerator();

	unsigned int getChannels();
	void generateBlock(AudioSample *output);
	void setPitchBend(double newPitchBend);
	void setLooping(bool looping);
	void setDecoder(std::shared_ptr<AudioDecoder> &decoder);
	void setPosition(double newPosition);

	private:
	void refillBuffer();
	/* Helper for pitch bend. */
	void nextSample(unsigned int channels, AudioSample *dest);
	void reallocateBuffers();

	std::shared_ptr<AudioDecoder> decoder = nullptr;
	std::shared_ptr<WDL_Resampler> resampler = nullptr;
	bool looping = false;
	double position = 0.0;
	double pitch_bend = 1.0;
	/*
	 * A buffer that holds data from the decoder. We refill and drain this repeatedly in order to deal with pitch bend.
	 * 
	 * This is bigger than it needs to be because we don't anticipate frequent seeks and it is better to take the file I/O and/or decoding hits in large amounts when the underlying decoder isn't PCM data.
	 * */
	static const unsigned int WORKING_BUFFER_SIZE = config::BLOCK_SIZE*10;
	AudioSample *working_buffer = nullptr;
	unsigned int working_buffer_used = 0;
	/*
	 * For pitch bend, the fractional offset that the last iteration used.
	 * */
	float fractional_offset = 0.0f;

	unsigned int allocated_channels = 0;
};

}