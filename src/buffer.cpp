#include "synthizer/buffer.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/math.hpp"
#include "synthizer/memory.hpp"

#include "WDL/resample.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace synthizer {

class DitherGenerator {
	public:
	AudioSample generate() {
		float r1 = this->distribution(this->engine);
		float r2 = this->distribution(this->engine);
		return 1.0f -r1 -r2;
	}

	private:
	std::mt19937 engine{10};
	std::uniform_real_distribution<float> distribution{0.0f, 1.0f };
};

/*
 * Takes a callable std::size_t producer(std::size_t frames, AudioSample *destination) and generates a buffer.
 * Producer must always return the number of samples requested exactly, unless the end of the buffer is reached, in which case it should rewturn less. We use 
 * the less condition to indicate that we're done.
 * */
template<typename T>
BufferData *generateBufferData(unsigned int channels, unsigned int sr, T &&producer) {
	DitherGenerator dither;
	WDL_Resampler *resampler = nullptr;
	std::vector<std::int16_t *> chunks;
	AudioSample *working_buf = nullptr;
	std::int16_t *next_chunk = nullptr;

	if (channels > config::MAX_CHANNELS) {
		throw ERange("Buffer has too many channels");
	}

	/* If the samplerates don't match, we'll have to go through a resampler. */
	if (sr != config::SR) {
		resampler = new WDL_Resampler();
		/* Sync interpolation. */
		resampler->SetMode(false, 0, true);
		resampler->SetRates(sr, config::SR);
		/* We're output driven. */
		resampler->SetFeedMode(false);
	}

	try {
		std::size_t chunk_size_samples = channels * config::BUFFER_CHUNK_SIZE;
		std::size_t length = 0;
		bool last = false;

		working_buf = new AudioSample[chunk_size_samples];

		while (last == false) {
			next_chunk = allocAligned<std::int16_t>(channels * config::BUFFER_CHUNK_SIZE);
			std::size_t next_chunk_len = 0;

			if (resampler != nullptr) {
				AudioSample *dst;
				auto needed = resampler->ResamplePrepare(config::BUFFER_CHUNK_SIZE, channels, &dst);
				auto got = producer(needed, dst);
				last = got < needed;
				next_chunk_len = resampler->ResampleOut(working_buf, got, config::BUFFER_CHUNK_SIZE, channels);
				assert(last == true || next_chunk_len == config::BUFFER_CHUNK_SIZE);
				length += next_chunk_len;
			} else {
				next_chunk_len = producer(config::BUFFER_CHUNK_SIZE, working_buf);
				last = next_chunk_len < config::BUFFER_CHUNK_SIZE;
			}

			for (std::size_t i = 0; i < next_chunk_len * channels; i++) {
				std::int_fast32_t tmp = working_buf[i] * 32768.0f + dither.generate();
				next_chunk[i] = clamp<std::int_fast32_t>(tmp, -32768, 32767);
			}

			std::fill(next_chunk + next_chunk_len * channels, next_chunk + chunk_size_samples, 0);
			chunks.push_back(next_chunk);
			length += next_chunk_len;
			/* In case the next allocation fails, avoid a double free. */
			next_chunk = nullptr;
		}

		auto ret = new BufferData(channels, length, std::move(chunks));
		delete[] working_buf;
		return ret;
	} catch(...) {
		for (auto x: chunks) {
			freeAligned(x);
		}
		delete resampler;
		freeAligned(next_chunk);
		delete[] working_buf;
		throw;
	}
}

std::shared_ptr<BufferData> bufferDataFromDecoder(const std::shared_ptr<AudioDecoder> &decoder) {
	auto channels = decoder->getChannels();
	auto sr = decoder->getSr();
	auto buf = generateBufferData(channels, sr, [&](auto frames, AudioSample *dest) {
		return decoder->writeSamplesInterleaved(frames, dest);
	});
	return std::shared_ptr<BufferData>(buf);
}

}


using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createBufferFromStream(syz_Handle *out, const char *protocol, const char *path, const char *options) {
	SYZ_PROLOGUE
	auto dec = getDecoderForProtocol(protocol, path, options);
	auto data = bufferDataFromDecoder(dec);
	auto buf = std::make_shared<Buffer>(data);
	*out = toC(buf);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetChannels(unsigned int *out, syz_Handle buffer) {
	SYZ_PROLOGUE
	auto b = fromC<Buffer>(buffer);
	*out = b->getChannels();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSamples(unsigned int *out, syz_Handle buffer) {
	SYZ_PROLOGUE
	auto b = fromC<Buffer>(buffer);
	*out = b->getLength();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSeconds(double *out, syz_Handle buffer) {
	SYZ_PROLOGUE
	auto b = fromC<Buffer>(buffer);
	*out = b->getLength() / (double) config::SR;
	return 0;
	SYZ_EPILOGUE
}
