#include <memory>
#include <cstdint>
#include <algorithm>

#include "dr_flac.h"

#include "synthizer/byte_stream.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

static std::size_t read_cb(void *user_data, void *out, std::size_t count) {
	ByteStream *stream = (ByteStream*)user_data;
	char *dest = (char *)out;
	auto ret = stream->read(count, dest);
	return ret;
}

static drflac_bool32 seek_cb(void *user_data, int offset, drflac_seek_origin origin) {
	ByteStream *stream = (ByteStream *)user_data;
	if (stream->supportsSeek() == false) {
		return DRFLAC_FALSE;
	}
	std::size_t desiredPos = origin == drflac_seek_origin_start ? offset : stream->getPosition() + offset;
	/* dr_flac can try to read past the end when seeking. */
	if (desiredPos > stream->getLength()) {
		return DRFLAC_FALSE;
	}
	stream->seek(desiredPos);
	return DRFLAC_TRUE;
}

class FlacDecoder: public AudioDecoder {
	public:
	FlacDecoder(std::shared_ptr<LookaheadByteStream> stream);
	~FlacDecoder();
	std::int64_t writeSamplesInterleaved(std::int64_t num, AudioSample *samples, std::int64_t channels = 0);
	int getSr();
	int getChannels();
	AudioFormat getFormat();
	void seekPcm(std::int64_t pos);
	bool supportsSeek();
	bool supportsSampleAccurateSeek();
	std::int64_t getLength();

	private:
	drflac *flac;
	std::shared_ptr<ByteStream> stream;
	AudioSample *tmp_buf = nullptr;

	static const int TMP_BUF_FRAMES = 1024;
};

FlacDecoder::FlacDecoder(std::shared_ptr<LookaheadByteStream> stream) {
	stream->resetFinal();
	this->stream = stream;

	this->flac = drflac_open(read_cb, seek_cb, stream.get(), NULL);
	if (this->flac == nullptr)
		throw Error("Unable to initialize flac stream");

	if(this->flac->channels == 0)
		throw Error("Got a flac file with 0 channels.");

	this->tmp_buf = new AudioSample[TMP_BUF_FRAMES*this->flac->channels];
}

FlacDecoder::~FlacDecoder() {
	drflac_close(this->flac);
	delete this->tmp_buf;
}

std::int64_t FlacDecoder::writeSamplesInterleaved(std::int64_t num, AudioSample *samples, std::int64_t channels) {
	auto actualChannels = channels < 1 ? this->flac->channels : channels;
	/* Fast case: if the channels are equal, just write. */
	if (actualChannels == this->flac->channels)
		return drflac_read_pcm_frames_f32(this->flac, num, samples);

	/* Otherwise we have to round trip via the temporary buffer. */
	std::int64_t got = drflac_read_pcm_frames_f32(this->flac, num, this->tmp_buf);
	std::fill(samples, samples + got * this->flac->channels, 0.0f);
	mixChannels(got, this->tmp_buf, this->flac->channels, samples, actualChannels);
	return got;
}

int FlacDecoder::getSr() {
	return this->flac->sampleRate;
}

int FlacDecoder::getChannels() {
	return this->flac->channels;
}

AudioFormat FlacDecoder::getFormat() {
	return AudioFormat::Flac;
}

void FlacDecoder::seekPcm(std::int64_t pos) {
	auto actualPos = std::min(this->getLength(), pos);
	if (drflac_seek_to_pcm_frame(this->flac, actualPos) == DRFLAC_FALSE)
		throw new Error("Unable to seek.");
}

bool FlacDecoder::supportsSeek() {
	return this->stream->supportsSeek();
}

bool FlacDecoder::supportsSampleAccurateSeek() {
	return this->supportsSeek() && true;
}

std::int64_t FlacDecoder::getLength() {
	return this->flac->totalPCMFrameCount;
}

std::shared_ptr<AudioDecoder> decodeFlac(std::shared_ptr<LookaheadByteStream> stream) {
	drflac *test_flac;

	test_flac = drflac_open(read_cb, seek_cb, stream.get(), NULL);
	if (test_flac == nullptr) {
		return nullptr;
	}

	drflac_close(test_flac);

	/* resetFinal handled by constructor. */
	return std::make_shared<FlacDecoder>(stream);
}

}