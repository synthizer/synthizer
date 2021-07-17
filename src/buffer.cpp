#include "synthizer/buffer.hpp"

#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/byte_stream.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/math.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/stream_handle.hpp"

#include "WDL/resample.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace synthizer {

class DitherGenerator {
	public:
	float generate() {
		float r1 = this->gen01();
		float r2 = this->gen01();
		return 1.0f -r1 -r2;
	}

	float gen01() {
		float o = gen.generateFloat();
		return (1.0 + o) * 0.5f;
	}

	private:
	RandomGenerator gen{};
};

/*
 * Takes a callable std::size_t producer(std::size_t frames, float *destination) and generates a buffer.
 * Producer must always return the number of samples requested exactly, unless the end of the buffer is reached, in which case it should rewturn less. We use 
 * the less condition to indicate that we're done.
 * */
template<typename T>
std::shared_ptr<BufferData> generateBufferData(unsigned int channels, unsigned int sr, T &&producer) {
	DitherGenerator dither;
	WDL_Resampler *resampler = nullptr;
	deferred_vector<std::int16_t *> chunks;
	float *working_buf = nullptr;
	std::int16_t *next_chunk = nullptr;

	if (channels > config::MAX_CHANNELS) {
		throw ERange("Buffer has too many channels");
	}

	/* If the samplerates don't match, we'll have to go through a resampler. */
	if (sr != config::SR) {
		resampler = new WDL_Resampler();
		/* Sync interpolation. */
		resampler->SetMode(false, 0, true, 20);
		resampler->SetRates(sr, config::SR);
		/* We're output driven. */
		resampler->SetFeedMode(false);
	}

	try {
		std::size_t chunk_size_samples = channels * config::BUFFER_CHUNK_SIZE;
		std::size_t length = 0;
		bool last = false;

		working_buf = new float[chunk_size_samples];

		while (last == false) {
			next_chunk = (std::int16_t *)calloc(channels * config::BUFFER_CHUNK_SIZE, sizeof(std::int16_t));
			std::size_t next_chunk_len = 0;

			if (resampler != nullptr) {
				float *dst;
				unsigned long long needed = resampler->ResamplePrepare(config::BUFFER_CHUNK_SIZE, channels, &dst);
				auto got = producer(needed, dst);
				last = got < needed;
				next_chunk_len = resampler->ResampleOut(working_buf, got, config::BUFFER_CHUNK_SIZE, channels);
				assert(last == true || next_chunk_len == config::BUFFER_CHUNK_SIZE);
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

		if (length == 0) {
			throw Error("Buffers of zero length not supported");
		}

		delete[] working_buf;
		return allocateSharedDeferred<BufferData>(channels, length, std::move(chunks));
	} catch(...) {
		for (auto x: chunks) {
			deferredFree(x);
		}
		delete resampler;
		deferredFree(next_chunk);
		delete[] working_buf;
		throw;
	}
}

std::shared_ptr<BufferData> bufferDataFromDecoder(const std::shared_ptr<AudioDecoder> &decoder) {
	auto channels = decoder->getChannels();
	auto sr = decoder->getSr();
	return generateBufferData(channels, sr, [&](auto frames, float *dest) {
		return decoder->writeSamplesInterleaved(frames, dest);
	});
}

int Buffer::getObjectType() {
	return SYZ_OTYPE_BUFFER;
}

}

using namespace synthizer;

/**
 * Part of the C API only. Create a buffer from a decoder and return the handle.
 * */
static syz_Handle bufferFromDecoder(std::shared_ptr<AudioDecoder> dec) {
	auto data = bufferDataFromDecoder(dec);
	auto buf = allocateSharedDeferred<Buffer>(data);
	auto ce = std::static_pointer_cast<CExposable>(buf);
	ce->stashInternalReference(ce);
	return toC(buf);
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamParams(syz_Handle *out, const char *protocol, const char *path, void *param, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	auto dec = getDecoderForStreamParams(protocol, path, param);
	*out = bufferFromDecoder(dec);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromEncodedData(syz_Handle *out, unsigned long long data_len, const char *data, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	auto stream = memoryStream(data_len, data);
	auto dec = getDecoderForStream(stream);
	*out = bufferFromDecoder(dec);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromFloatArray(syz_Handle *out, unsigned int sr, unsigned int channels, unsigned long long frames, const float *data, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	if (channels > config::MAX_CHANNELS) {
		throw ERange("Too many channels");
	}
	auto dec = getRawDecoder(sr, channels, frames, data);
	*out = bufferFromDecoder(dec);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromFile(syz_Handle *out, const char *path, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	return syz_createBufferFromStreamParams(out, "file", path, NULL, userdata, userdata_free_callback);
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamHandle(syz_Handle *out, syz_Handle stream, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	auto s = fromC<StreamHandle>(stream);
	auto bs = consumeStreamHandle(s);
	auto dec = getDecoderForStream(bs);
	*out = bufferFromDecoder(dec);
	return syz_handleSetUserdata(0, userdata, userdata_free_callback);
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
