#include <memory>
#include <cstdint>
#include <algorithm>

#include "dr_mp3.h"

#include "synthizer/byte_stream.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/types.hpp"

namespace synthizer {

static std::size_t read_cb(void *user_data, void *out, std::size_t count) {
	ByteStream *stream = (ByteStream*)user_data;
	auto ret = stream->read(count, (char *)out);
	return ret;
}

static drmp3_bool32 seek_cb(void *user_data, int offset, drmp3_seek_origin origin) {
	ByteStream *stream = (ByteStream *)user_data;
	stream->seek(origin ==     drmp3_seek_origin_start ? offset : stream->getPosition() + offset);
	return DRMP3_TRUE;
}

class Mp3Decoder: public AudioDecoder {
	public:
	Mp3Decoder(std::shared_ptr<LookaheadByteStream> stream);
	~Mp3Decoder();
	std::int64_t writeSamplesInterleaved(std::int64_t num, AudioSample *samples, std::int64_t channels = 0);
	int getSr();
	int getChannels();
	AudioFormat getFormat();
	void seekPcm(std::int64_t pos);
	bool supportsSeek();
	bool supportsSampleAccurateSeek();
	std::int64_t getLength();

	private:
	drmp3 mp3;
	std::shared_ptr<ByteStream> stream;
	AudioSample *tmp_buf = nullptr;
	std::int64_t frame_count = 0;

	static const int TMP_BUF_FRAMES = 1024;
};

Mp3Decoder::Mp3Decoder(std::shared_ptr<LookaheadByteStream> stream) {
	stream->resetFinal();
	this->stream = stream;

	if (drmp3_init(&this->mp3, read_cb,
		this->stream->supportsSeek() ? seek_cb : NULL, this->stream.get(), NULL) == DRMP3_FALSE) {
		throw Error("Unable to initialize Mp3 stream");
	}

	if(this->mp3.channels == 0) {
		throw Error("Got a MP3 file with 0 channels.");
	}

	if (stream->supportsSeek()) {
		this->frame_count = drmp3_get_pcm_frame_count(&this->mp3);
		if (this->frame_count == 0) {
			throw Error("Stream supports seek, but unable to compute frame count for Mp3 stream");
		}
	}

	this->tmp_buf = new AudioSample[TMP_BUF_FRAMES*this->mp3.channels];
}

Mp3Decoder::~Mp3Decoder() {
	drmp3_uninit(&this->mp3);
	delete this->tmp_buf;
}

std::int64_t Mp3Decoder::writeSamplesInterleaved(std::int64_t num, AudioSample *samples, std::int64_t channels) {
	auto actualChannels = channels < 1 ? this->mp3.channels : channels;
	/* Fast case: if the channels are equal, just write. */
	if (actualChannels == this->mp3.channels) {
		return drmp3_read_pcm_frames_f32(&this->mp3, num, samples);
	}

	/* Otherwise we have to round trip via the temporary buffer. */
	std::int64_t got = drmp3_read_pcm_frames_f32(&this->mp3, num, this->tmp_buf);
	mixChannels(got, this->tmp_buf, this->mp3.channels, samples, actualChannels);
	return got;
}

int Mp3Decoder::getSr() {
	return this->mp3.sampleRate;
}

int Mp3Decoder::getChannels() {
	return this->mp3.channels;
}

AudioFormat Mp3Decoder::getFormat() {
	return AudioFormat::Mp3;
}

void Mp3Decoder::seekPcm(std::int64_t pos) {
	auto actualPos = std::min(this->getLength(), pos);
	if (drmp3_seek_to_pcm_frame(&this->mp3, actualPos) == DRMP3_FALSE)
		throw new Error("Unable to seek.");
}

bool Mp3Decoder::supportsSeek() {
	return this->stream->supportsSeek();
}

bool Mp3Decoder::supportsSampleAccurateSeek() {
	return this->supportsSeek() && true;
}

std::int64_t Mp3Decoder::getLength() {
	return this->frame_count;
}

std::shared_ptr<AudioDecoder> decodeMp3(std::shared_ptr<LookaheadByteStream> stream) {
	drmp3 test_mp3;

	if (drmp3_init(&test_mp3, read_cb, NULL, (void*)stream.get(), NULL) == DRMP3_FALSE)
		return nullptr;

	drmp3_uninit(&test_mp3);

	/* resetFinal handled by constructor. */
	return std::make_shared<Mp3Decoder>(stream);
}

}