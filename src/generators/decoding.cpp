#include "synthizer/generators/decoding.hpp"

#include "synthizer/config.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/memory.hpp"

#include "WDL/resample.h"

#include <algorithm>
#include <memory>

namespace synthizer {

DecodingGenerator::~DecodingGenerator() {
	delete[] this->working_buffer;
}

unsigned int DecodingGenerator::getChannels() {
	unsigned int c = this->decoder ? this->decoder->getChannels() : 0;
	return std::min<unsigned int>(c, config::MAX_CHANNELS);
}

void DecodingGenerator::generateBlock(AudioSample *output) {
	if (this->decoder == nullptr || this->pitch_bend <= 0.0) {
		return;
	}

	unsigned int channels = this->getChannels();
	if (this->pitch_bend == 1.0) {
		/* Boring copy with periodic refills. */
		unsigned int needed = config::BLOCK_SIZE;
		while (needed > 0) {
			unsigned int remaining= WORKING_BUFFER_SIZE - this->working_buffer_used;
			unsigned int will_copy = needed > remaining ? remaining : needed;
			for(unsigned int i = 0; i < will_copy; i++) {
				float *buf = &this->working_buffer[channels*(this->working_buffer_used+i)];
				for(unsigned int j = 0; j < channels; j++) output[i*channels+j] += buf[j];
			}
			output += will_copy*channels;
			needed -= will_copy;
			this->working_buffer_used += will_copy;
			if(this->working_buffer_used >= WORKING_BUFFER_SIZE) {
				this->refillBuffer();
			}
		}
	} else {
		/* Pitch bend. */
		float fractional_offset = this->fractional_offset;
		float delta = this->pitch_bend;
		/* Current and next sample; we work out which is which via the flag. */
		std::array<AudioSample, config::MAX_CHANNELS> samples[2];
		unsigned int cur_sample = 0;
		std::copy(this->working_buffer + this->working_buffer_used*channels, this->working_buffer + this->working_buffer_used*channels + channels, samples[0].data());
		this->nextSample(channels, samples[1].data());
		for(unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
			float w1 = 1.0f-fractional_offset;
			float w2 = fractional_offset;
			float *pf1 = samples[cur_sample].data();
			float *pf2 = samples[cur_sample^1].data();
			for(unsigned int j =0; j < channels; j++) output[j] += pf1[j]*w1+pf2[j]*w2;
			output += channels;
			fractional_offset += delta;
			if (fractional_offset > 1.0f) {
				/* TODO: optimize this. */
				unsigned int advances = fractional_offset;
				fractional_offset -= advances;
				while(advances) {
					this->nextSample(channels, samples[cur_sample].data());
					/* Flip cur_sample. */
					cur_sample ^= 1;
					advances--;
				}
			}
		}
		this->fractional_offset = fractional_offset;
	}
}

void DecodingGenerator::nextSample(unsigned int channels, AudioSample *dest) {
	this->working_buffer_used += 1;
	if(this->working_buffer_used >= WORKING_BUFFER_SIZE) {
		this->refillBuffer();
	}
	std::copy(this->working_buffer + this->working_buffer_used * channels, this->working_buffer + this->working_buffer_used * channels + channels, dest);
}

void DecodingGenerator::setPitchBend(double newPitchBend) {
	this->pitch_bend = newPitchBend;
}

void DecodingGenerator::setLooping(bool looping) {
	this->looping = looping;
}

void DecodingGenerator::setDecoder(std::shared_ptr<AudioDecoder> &decoder) {
	this->decoder = decoder;
	this->resampler = nullptr;
	double old_sr = decoder->getSr();
	if (old_sr != config::SR) {
		this->resampler = std::make_shared<WDL_Resampler>();
		/* Configure resampler to use sinc filters and have required sample rates. */
		this->resampler->SetMode(false, 0, true);
		this->resampler->SetRates(old_sr, config::SR);
	}
	this->reallocateBuffers();
}

void DecodingGenerator::setPosition(double pos) {
	if(this->decoder) this->decoder->seekSeconds(pos);
}

static void fillBufferFromDecoder(AudioDecoder &decoder, unsigned int size, unsigned int channels, float *dest, bool looping) {
	unsigned int needed = size;
	while (needed) {
		unsigned int got = decoder.writeSamplesInterleaved(needed, dest);
		dest += channels*got;
		needed -= got;
		dest += got*channels;
		if (needed > 0 && looping && decoder.supportsSeek()) {
			decoder.seekSeconds(0.0);
		} else {
			break;
		}
	}
	std::fill(dest, dest + needed*channels, 0.0f);
}

void DecodingGenerator::refillBuffer() {
	if (this->decoder == nullptr) {
		return;
	}

	if (this->resampler == nullptr) {
		fillBufferFromDecoder(*this->decoder, WORKING_BUFFER_SIZE, this->getChannels(), working_buffer, this->looping);
	} else {
		float *rs_buf;
		int needed = this->resampler->ResamplePrepare(WORKING_BUFFER_SIZE, this->getChannels(), &rs_buf);
		fillBufferFromDecoder(*this->decoder, needed, this->getChannels(), rs_buf, this->looping);
		unsigned int resampled = this->resampler->ResampleOut(this->working_buffer, needed, WORKING_BUFFER_SIZE, this->getChannels());
		if(resampled < WORKING_BUFFER_SIZE) {
			std::fill(working_buffer + resampled * this->getChannels(), working_buffer + WORKING_BUFFER_SIZE * this->getChannels(), 0.0f);
		}
	}

	this->working_buffer_used = 0;
}

void DecodingGenerator::reallocateBuffers() {
	unsigned int nch = this->getChannels();
	if(nch <= this->allocated_channels) return;

	delete[] this->working_buffer;

	this->working_buffer = new AudioSample[nch*WORKING_BUFFER_SIZE];
	this->allocated_channels = nch;
	this->working_buffer_used = WORKING_BUFFER_SIZE;
}

}