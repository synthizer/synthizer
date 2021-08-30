#include "synthizer/byte_stream.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/types.hpp"

#include "dr_flac.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace synthizer {

static std::size_t read_cb(void *user_data, void *out, std::size_t count) {
  ByteStream *stream = (ByteStream *)user_data;
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

class FlacDecoder : public AudioDecoder {
public:
  FlacDecoder(std::shared_ptr<LookaheadByteStream> stream);
  ~FlacDecoder();
  unsigned long long writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int channels = 0);
  int getSr();
  int getChannels();
  AudioFormat getFormat();
  void seekPcm(unsigned long long pos);
  bool supportsSeek();
  bool supportsSampleAccurateSeek();
  unsigned long long getLength();

private:
  drflac *flac;
  std::shared_ptr<ByteStream> stream;
  std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> tmp_buf{{0.0f}};
};

FlacDecoder::FlacDecoder(std::shared_ptr<LookaheadByteStream> stream) {
  stream->resetFinal();
  this->stream = stream;

  this->flac = drflac_open(read_cb, seek_cb, stream.get(), NULL);
  if (this->flac == nullptr)
    throw Error("Unable to initialize flac stream");

  if (this->flac->channels == 0) {
    throw Error("Got a flac file with 0 channels.");
  }
  if (this->flac->channels >= config::MAX_CHANNELS) {
    throw Error("Got a flac file with too many channels");
  }
}

FlacDecoder::~FlacDecoder() { drflac_close(this->flac); }

unsigned long long FlacDecoder::writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int channels) {
  auto actualChannels = channels < 1 ? this->flac->channels : channels;
  /* Fast case: if the channels are equal, just write. */
  if (actualChannels == this->flac->channels) {
    return drflac_read_pcm_frames_f32(this->flac, num, samples);
  }

  /* Otherwise we have to round trip via the temporary buffer. */
  std::fill(samples, samples + num * actualChannels, 0.0f);

  unsigned long long needed = num;
  unsigned long long tmp_buf_frames = this->tmp_buf.size() / this->flac->channels;
  while (needed > 0) {
    unsigned long long will_read = std::min(tmp_buf_frames, needed);
    unsigned long long got = drflac_read_pcm_frames_f32(this->flac, will_read, &this->tmp_buf[0]);
    if (got == 0) {
      break;
    }
    needed -= got;
    mixChannels(got, &this->tmp_buf[0], this->flac->channels, samples + (num - needed) * actualChannels,
                actualChannels);
  }

  return num - needed;
}

int FlacDecoder::getSr() { return this->flac->sampleRate; }

int FlacDecoder::getChannels() { return this->flac->channels; }

AudioFormat FlacDecoder::getFormat() { return AudioFormat::Flac; }

void FlacDecoder::seekPcm(unsigned long long pos) {
  auto actualPos = std::min(this->getLength(), pos);
  if (drflac_seek_to_pcm_frame(this->flac, actualPos) == DRFLAC_FALSE)
    throw new Error("Unable to seek.");
}

bool FlacDecoder::supportsSeek() { return this->stream->supportsSeek(); }

bool FlacDecoder::supportsSampleAccurateSeek() { return this->supportsSeek() && true; }

unsigned long long FlacDecoder::getLength() { return this->flac->totalPCMFrameCount; }

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

} // namespace synthizer