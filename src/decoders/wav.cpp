#include "synthizer/byte_stream.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/types.hpp"

#include "dr_wav.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace synthizer {

static std::size_t read_cb(void *user_data, void *out, std::size_t count) {
	ByteStream *stream = (ByteStream*)user_data;
	auto ret = stream->read(count, (char *)out);
	return ret;
}

static drwav_bool32 seek_cb(void *user_data, int offset, drwav_seek_origin origin) {
	ByteStream *stream = (ByteStream *)user_data;
	stream->seek(origin ==     drwav_seek_origin_start ? offset : stream->getPosition() + offset);
	return DRWAV_TRUE;
}

class WavDecoder: public AudioDecoder {
	public:
	WavDecoder(std::shared_ptr<LookaheadByteStream> stream);
	~WavDecoder();
	unsigned long long writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int channels = 0);
	int getSr();
	int getChannels();
	AudioFormat getFormat();
	void seekPcm(unsigned long long pos);
	bool supportsSeek();
	bool supportsSampleAccurateSeek();
	unsigned long long getLength();

	private:
	drwav wav;
	std::shared_ptr<ByteStream> stream;
	std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> tmp_buf{{0.0f}};
};

WavDecoder::WavDecoder(std::shared_ptr<LookaheadByteStream> stream) {
	stream->resetFinal();
	this->stream = stream;
	drwav_uint32 flags = 0;
	if (stream->supportsSeek() == false) {
		flags = DRWAV_SEQUENTIAL;
	}

	if (drwav_init_ex(&this->wav, read_cb, seek_cb, NULL, this->stream.get(), NULL, flags, NULL) == DRWAV_FALSE) {
		throw Error("Unable to initialize wav stream");
	}

	if(this->wav.channels == 0) {
		drwav_uninit(&this->wav);
		throw Error("Got a wave file with 0 channels.");
	}
	if (this->wav.channels > config::MAX_CHANNELS) {
		drwav_uninit(&this->wav);
		throw Error("Too many channels");
	}
}

WavDecoder::~WavDecoder() {
	drwav_uninit(&this->wav);
}

unsigned long long WavDecoder::writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int channels) {
	auto actualChannels = channels < 1 ? this->wav.channels : channels;
	/* Fast case: if the channels are equal, just write. */
	if (actualChannels == this->wav.channels)
		return drwav_read_pcm_frames_f32(&this->wav, num, samples);

	/* Otherwise we have to round trip via the temporary buffer. */
	std::fill(samples, samples + num * actualChannels, 0.0f);
	unsigned long long needed = num;
	unsigned long long tmp_buf_frames = this->tmp_buf.size() / this->wav.channels;
	while (needed > 0) {
		unsigned long long will_read = std::min(needed, tmp_buf_frames);
		unsigned long long got = drwav_read_pcm_frames_f32(&this->wav, will_read, &this->tmp_buf[0]);
		needed -= got;
		if (got == 0) {
			break;
		}
		mixChannels(got, &this->tmp_buf[0], this->wav.channels, samples + (num - needed) * actualChannels, actualChannels);
	}

	return num - needed;
}

int WavDecoder::getSr() {
	return this->wav.sampleRate;
}

int WavDecoder::getChannels() {
	return this->wav.channels;
}

AudioFormat WavDecoder::getFormat() {
	return AudioFormat::Wav;
}

void WavDecoder::seekPcm(unsigned long long pos) {
	auto actualPos = std::min(this->getLength(), pos);
	if (drwav_seek_to_pcm_frame(&this->wav, actualPos) == DRWAV_FALSE)
		throw new Error("Unable to seek.");
}

bool WavDecoder::supportsSeek() {
	return this->stream->supportsSeek();
}

bool WavDecoder::supportsSampleAccurateSeek() {
	return this->supportsSeek() && true;
}

unsigned long long WavDecoder::getLength() {
	return this->wav.totalPCMFrameCount;
}

std::shared_ptr<AudioDecoder> decodeWav(std::shared_ptr<LookaheadByteStream> stream) {
	drwav test_wav;
	drwav_uint32 flags = 0;

	/* Disable seeking if necessary. */
	if (stream->supportsSeek() == false)	
		flags = DRWAV_SEQUENTIAL;

	if (drwav_init_ex(&test_wav, read_cb, seek_cb, NULL, (void*)stream.get(), NULL, flags, NULL) == DRWAV_FALSE)
		return nullptr;

	drwav_uninit(&test_wav);

	/* resetFinal handled by constructor. */
	return std::make_shared<WavDecoder>(stream);
}

}