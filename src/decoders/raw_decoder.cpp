#include "synthizer/decoding.hpp"

#include "synthizer/memory.hpp"

#include <memory>

namespace synthizer {

class RawDecoder: public AudioDecoder {
	public:
	RawDecoder(std::size_t sr, unsigned int channels, std::size_t frames, float *data): channels(channels), sr(sr), data(data), frames(frames) {}

	std::int64_t writeSamplesInterleaved(std::int64_t num, float *samples, std::int64_t channels = 0) override;
	void seekPcm(std::int64_t frame) override;
	bool supportsSeek() override;
	bool supportsSampleAccurateSeek() override;
	std::int64_t getLength() override;
	int getSr() override;
	int getChannels() override;
	AudioFormat getFormat() override;

	private:
	float *data;
	std::size_t frames;
	unsigned int channels;
	unsigned int sr;
	std::size_t position_in_frames = 0;
};

std::int64_t RawDecoder::writeSamplesInterleaved(std::int64_t num, float *samples, std::int64_t channels) {
	if (this->position_in_frames >= this->frames) {
		return 0;
	}

	std::size_t remaining_frames = this->frames - this->position_in_frames;
	std::size_t will_read = num > remaining_frames ? remaining_frames : num;
	if (channels == 0) channels = this->channels;
	std::size_t channels_out = this->channels > channels ? this->channels : channels;

	if (channels_out == this->channels) {
		std::copy(this->data + this->position_in_frames * this->channels, this->data + (this->position_in_frames + will_read) * this->channels, samples);
	} else if (channels_out > this->channels) {
		for (unsigned int i = 0; i < will_read; i++) {
			float *frame = this->data + (this->position_in_frames + i) * this->channels;
			float *out_frame = samples + i * channels_out;
			for (unsigned int j = 0; j < this->channels; j++) {
				out_frame[j] = frame[j];
			}
			std::fill(out_frame + this->channels, out_frame + channels_out, 0.0f);
		}
	} else {
		for (unsigned int i = 0; i < will_read; i++) {
			float *frame = this->data + (this->position_in_frames + i) * this->channels;
			float *out_frame = samples + i * channels_out;
			for (unsigned int j = 0; j < channels_out; j++) {
				out_frame[j] = frame[j];
			}
		}
	}

	std::fill(samples + will_read * channels_out, samples + num * channels_out, 0.0f);
	this->position_in_frames += will_read;
	return will_read;
}

void RawDecoder::seekPcm(std::int64_t frame) {
	this->position_in_frames = frame;
}

bool RawDecoder::supportsSeek() {
	return true;
}

bool RawDecoder::supportsSampleAccurateSeek() {
	return true;
}

std::int64_t RawDecoder::getLength() {
	return this->frames;
}

int RawDecoder::getSr() {
	return this->sr;
}

int RawDecoder::getChannels() {
	return this->channels;
}

AudioFormat RawDecoder::getFormat() {
	return AudioFormat::Raw;
}

std::shared_ptr<AudioDecoder> getRawDecoder(unsigned int sr, unsigned int channels, std::size_t frames, float *data) {
	RawDecoder *dec = new RawDecoder(sr, channels, frames, data);
	return sharedPtrDeferred<RawDecoder>(dec, deferredDelete<RawDecoder>);
}

}
