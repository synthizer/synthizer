#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/config.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/math.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/types.hpp"

#include "WDL/resample.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace synthizer {
class Context;

/**
 * Infrastructures for buffers.
 *
 * This involves 3 entities:
 *
 * - Bufferdata holds data for a buffer.
 * - Buffer is the user-exposed object wrapping a BufferData.
 * - BufferReader reads from a BufferData.
 *
 * Note that BufferReader doesn't use shared_ptr or weak_ptr; it's effectively "borrowing" to use Rust terminology.
 * */

class BufferData {
public:
  BufferData(unsigned int channels, deferred_vector<std::int16_t> &&data) : channels(channels), data(std::move(data)) {}

  unsigned int getChannels() const { return this->channels; }

  std::size_t getLength() const { return this->data.size(); }

  /**
   * Get a pointer to a slice of the data.
   *
   * This takes end so that it can assert that we aren't trying to read past the end in debug builds.
   * */
  const std::int16_t *getPointerToSubslice(std::size_t start, std::size_t end) const {
    (void)end;
    assert(end <= this->data.size());
    return &this->data[start];
  }

private:
  unsigned int channels;
  deferred_vector<std::int16_t> data;
};

class Buffer : public CExposable {
public:
  Buffer(const std::shared_ptr<BufferData> &b) : data(b) {}

  unsigned int getChannels() const { return this->data->getChannels(); }

  std::size_t getLength() const { return this->data->getLength(); }

  int getObjectType() override;

  const BufferData *getData() { return this->data.get(); }

private:
  std::shared_ptr<BufferData> data;
};

class BufferReader {
public:
  BufferReader() : buffer(nullptr) {}

  BufferReader(Buffer *b) { this->setBuffer(b); }

  void setBuffer(Buffer *buf) {
    this->buffer = buf;
    assert(this->buffer->getChannels() < config::MAX_CHANNELS);
  }

  unsigned int getChannels() const { return this->buffer->getChannels(); }

  std::size_t getLength() const { return this->buffer->getLength(); }

  const BufferData *getData() const { return this->buffer->getData(); }

  std::int16_t readSampleI16(std::size_t pos, unsigned int channel) const {
    assert(channel < this->buffer->getChannels());
    const std::int16_t *ptr = this->getData()->getPointerToSubslice(pos + channel, pos + channel + 1);
    return *ptr;
  }

  float readSample(std::size_t pos, unsigned int channel) { return this->readSampleI16(pos, channel) / 32768.0f; }

  void readFrameI16(std::size_t pos, std::int16_t *out) {
    const std::int16_t *ptr = this->getData()->getPointerToSubslice(pos, pos + this->getChannels());
    std::copy(ptr, ptr + this->getChannels(), out);
  }

  void readFrame(std::size_t pos, float *out) {
    std::array<std::int16_t, config::MAX_CHANNELS> intermediate;
    readFrameI16(pos, &intermediate[0]);
    for (unsigned int i = 0; i < this->getChannels(); i++) {
      out[i] = intermediate[i] / 32768.0f;
    }
  }

  /* Returns frames read. This will be less than requested at the end of the buffer. */
  std::size_t readFramesI16(std::size_t pos, std::size_t count, std::int16_t *out) {
    if (pos * this->getChannels() >= this->getLength()) {
      return 0;
    }

    std::size_t available = (this->getLength() - pos) / this->getChannels();
    std::size_t will_read = available < count ? available : count;
    const std::int16_t *ptr =
        this->getData()->getPointerToSubslice(pos * this->getChannels(), (pos + will_read) * this->getChannels());
    std::copy(ptr, ptr + will_read * this->getChannels(), out);
    return will_read;
  }

  std::size_t readFrames(std::size_t pos, std::size_t count, float *out) {
    std::array<std::int16_t, config::MAX_CHANNELS * 16> workspace = {0};
    std::size_t workspace_frames = workspace.size() / this->getChannels();

    std::size_t written = 0;
    float *cursor = out;

    while (written < count) {
      std::size_t requested = std::min(count - written, workspace_frames);
      std::size_t got = this->readFramesI16(pos + written, requested, &workspace[0]);
      for (unsigned int i = 0; i < got * this->getChannels(); i++) {
        cursor[i] = workspace[i] / 32768.0f;
      }
      cursor += got * this->getChannels();
      written += got;
      if (got < requested) {
        break;
      }
    }

    return written;
  }

  /**
   * get a raw view on the internal buffer.
   *
   * This is used in BufferGenerator to optimize a common case where we can simply copy by folding the conversion to f32
   * in and entirely avoiding a temporary uffer.
   * */
  const std::int16_t *getRawSlice(std::size_t start, std::size_t end) const {
    return this->getData()->getPointerToSubslice(start, end);
  }

private:
  Buffer *buffer = nullptr;
};

std::shared_ptr<BufferData> bufferDataFromDecoder(const std::shared_ptr<AudioDecoder> &decoder);

class DitherGenerator {
public:
  float generate() {
    float r1 = this->gen01();
    float r2 = this->gen01();
    return 1.0f - r1 - r2;
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
 * Producer must always return the number of samples requested exactly, unless the end of the buffer is reached, in
 * which case it should rewturn less. We use the less condition to indicate that we're done.
 * */
template <typename T>
inline std::shared_ptr<BufferData> generateBufferData(unsigned int channels, unsigned int sr, T &&producer) {
  DitherGenerator dither;
  WDL_Resampler *resampler = nullptr;
  deferred_vector<std::int16_t> data;
  float *working_buf = nullptr;

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
    bool last = false;
    std::size_t chunk_size_samples = channels * config::BUFFER_DECODE_CHUNK_SIZE;
    working_buf = new float[chunk_size_samples];

    while (last == false) {
      std::size_t next_chunk_len = 0;

      if (resampler != nullptr) {
        float *dst;
        unsigned long long needed = resampler->ResamplePrepare(config::BUFFER_DECODE_CHUNK_SIZE, channels, &dst);
        auto got = producer(needed, dst);
        last = got < needed;
        next_chunk_len = resampler->ResampleOut(working_buf, got, config::BUFFER_DECODE_CHUNK_SIZE, channels);
        assert(last == true || next_chunk_len == config::BUFFER_DECODE_CHUNK_SIZE);
      } else {
        next_chunk_len = producer(config::BUFFER_DECODE_CHUNK_SIZE, working_buf);
        last = next_chunk_len < config::BUFFER_DECODE_CHUNK_SIZE;
      }

      for (std::size_t i = 0; i < next_chunk_len * channels; i++) {
        std::int_fast32_t tmp = working_buf[i] * 32768.0f + dither.generate();
        data.push_back(clamp<std::int_fast32_t>(tmp, -32768, 32767));
      }
    }

    if (data.size() == 0) {
      throw Error("Buffers of zero length not supported");
    }

    delete[] working_buf;
    return allocateSharedDeferred<BufferData>(channels, std::move(data));
  } catch (...) {
    delete resampler;
    delete[] working_buf;
    throw;
  }
}

inline std::shared_ptr<BufferData> bufferDataFromDecoder(const std::shared_ptr<AudioDecoder> &decoder) {
  auto channels = decoder->getChannels();
  auto sr = decoder->getSr();
  return generateBufferData(channels, sr,
                            [&](auto frames, float *dest) { return decoder->writeSamplesInterleaved(frames, dest); });
}

inline int Buffer::getObjectType() { return SYZ_OTYPE_BUFFER; }

} // namespace synthizer
