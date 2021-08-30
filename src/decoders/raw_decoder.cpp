#include "synthizer/decoding.hpp"

#include "synthizer/memory.hpp"

#include <memory>

namespace synthizer {

class RawDecoder : public AudioDecoder {
public:
  RawDecoder(std::size_t sr, unsigned int channels, std::size_t frames, const float *data)
      : data(data), frames(frames), channels(channels), sr(sr) {}

  unsigned long long writeSamplesInterleaved(unsigned long long num, float *samples,
                                             unsigned int channels = 0) override;
  void seekPcm(unsigned long long frame) override;
  bool supportsSeek() override;
  bool supportsSampleAccurateSeek() override;
  unsigned long long getLength() override;
  int getSr() override;
  int getChannels() override;
  AudioFormat getFormat() override;

private:
  const float *data;
  std::size_t frames;
  unsigned int channels;
  unsigned int sr;
  std::size_t position_in_frames = 0;
};

unsigned long long RawDecoder::writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int chans) {
  if (this->position_in_frames >= this->frames) {
    return 0;
  }

  std::size_t remaining_frames = this->frames - this->position_in_frames;
  std::size_t will_read = (std::size_t)num > remaining_frames ? remaining_frames : num;
  if (chans == 0)
    chans = this->channels;
  std::size_t channels_out = this->channels > chans ? this->channels : chans;

  if (channels_out == this->channels) {
    std::copy(this->data + this->position_in_frames * this->channels,
              this->data + (this->position_in_frames + will_read) * this->channels, samples);
  } else if (channels_out > this->channels) {
    for (unsigned int i = 0; i < will_read; i++) {
      const float *frame = this->data + (this->position_in_frames + i) * this->channels;
      float *out_frame = samples + i * channels_out;
      for (unsigned int j = 0; j < this->channels; j++) {
        out_frame[j] = frame[j];
      }
      std::fill(out_frame + this->channels, out_frame + channels_out, 0.0f);
    }
  } else {
    for (unsigned int i = 0; i < will_read; i++) {
      const float *frame = this->data + (this->position_in_frames + i) * this->channels;
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

void RawDecoder::seekPcm(unsigned long long frame) { this->position_in_frames = frame; }

bool RawDecoder::supportsSeek() { return true; }

bool RawDecoder::supportsSampleAccurateSeek() { return true; }

unsigned long long RawDecoder::getLength() { return this->frames; }

int RawDecoder::getSr() { return this->sr; }

int RawDecoder::getChannels() { return this->channels; }

AudioFormat RawDecoder::getFormat() { return AudioFormat::Raw; }

std::shared_ptr<AudioDecoder> getRawDecoder(unsigned int sr, unsigned int channels, std::size_t frames,
                                            const float *data) {
  RawDecoder *dec = new RawDecoder(sr, channels, frames, data);
  return sharedPtrDeferred<RawDecoder>(dec, deferredDelete<RawDecoder>);
}

} // namespace synthizer
