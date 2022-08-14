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

/*
 * Buffers hold decoded audio data as 16-bit samples resampled to the Synthizer samplerate.  The following entities are
 * involved:
 *
 * - A BufferData holds the data itself.
 * - A Buffer holds a reference to a BufferData.
 * - A BufferChunk holds a non-owning reference to a range of data inside a buffer.
 *
 * A BufferReader type is also provided, which provides convenience methods for reading without having to deal with
 * assembling objects yourself.
 *
 * Design justifications/explanations:
 *
 * - We use 16-bit samples because this is enough for audio playback, and it cuts ram usage in half w.r.t. floats.
 * - We store data in noncontiguous chunks in order to assist low memory systems, and also because this makes it harder
 * for someone to just copy audio data out with a debugger, since doing so would require walking our internal
 * structures. Also, branch prediction should make reading cheap.
 * - We resample first for CPU usage purposes.
 * - A Buffer and BufferData are separate types because we can expose Buffers through the C API, which will allow us to
 * offer a BufferCache in future, rather than making everyone implement this themselves.
 *
 * Most of this file is inline because otherwise things get expensive very quickly short of compiling with LTO and
 * optimization and it would be nice to be able to use these in debug builds.
 * */

/* Helper type to hold a pointer and a length for buffer chunks. */
class BufferChunk {
public:
  std::size_t start = 0;
  std::size_t end = 0;
  std::int16_t *data = nullptr;
};

/*
 * A BufferData holds the actual audio data, and is simply a vector of pointers.
 * */
class BufferData {
public:
  /* The chunks are built by a decoder or something else, then fed here. */
  BufferData(unsigned int channels, std::size_t length, deferred_vector<std::int16_t *> &&chunks)
      : channels(channels), length(length), chunks(std::move(chunks)) {}

  ~BufferData() {
    for (auto c : this->chunks) {
      deferredFree(c);
    }
  }

  unsigned int getChannels() const { return this->channels; }

  std::size_t getLength() const { return this->length; }

  BufferChunk getChunk(std::size_t index) const {
    std::size_t actual_index = index / config::BUFFER_CHUNK_SIZE;
    std::size_t rounded_index = actual_index * config::BUFFER_CHUNK_SIZE;
    return {
        /*.start=*/rounded_index,
        /* Note: this is more complicated because it has to account for a potential partial page at the end. */
        /*.end=*/rounded_index + std::min(this->length - rounded_index, config::BUFFER_CHUNK_SIZE),
        /*.data=*/this->chunks[actual_index],
    };
  }

private:
  unsigned int channels;
  std::size_t length;
  deferred_vector<std::int16_t *> chunks;
};

class Context;

class Buffer : public CExposable {
public:
  Buffer(const std::shared_ptr<BufferData> &b) : data(b) {}

  unsigned int getChannels() const { return this->data->getChannels(); }

  std::size_t getLength() const { return this->data->getLength(); }

  BufferChunk getChunk(std::size_t index) const { return this->data->getChunk(index); }

  int getObjectType() override;

private:
  std::shared_ptr<BufferData> data;
};

/*
 * Very, very, very important note: this doesn't hold a shared_ptr.
 *
 * This is because it's used by BufferGenerator, which intentionally needs to hold a weak reference to play nice with
 * the object property subsystem.
 * */
class BufferReader {
public:
  BufferReader() : buffer(nullptr) {}

  BufferReader(Buffer *b) { this->setBuffer(b); }

  void setBuffer(Buffer *buf) {
    this->buffer = buf;
    this->channels = this->buffer->getChannels();
    this->length = buffer->getLength();
    assert(this->channels < config::MAX_CHANNELS);
    this->chunk = {};
  }

  unsigned int getChannels() const { return this->channels; }

  std::size_t getLength() const { return this->length; }

  std::int16_t readSampleI16(std::size_t pos, unsigned int channel) {
    assert(channel < this->channels);
    /* Fast case. */
    if (this->chunk.start <= pos && pos < this->chunk.end) {
      goto do_read;
    } else if (pos >= this->length) {
      return 0;
    } else {
      this->chunk = this->buffer->getChunk(pos);
      goto do_read;
    }
  do_read:
    return this->chunk.data[(pos - this->chunk.start) * this->channels + channel];
  }

  float readSample(std::size_t pos, unsigned int channel) { return this->readSampleI16(pos, channel) / 32768.0f; }

  void readFrameI16(std::size_t pos, std::int16_t *out) {
    if (this->chunk.start <= pos && pos < this->chunk.end) {
      goto do_read;
    } else if (pos > this->length) {
      std::fill(out, out + this->channels, 0);
      return;
    } else {
      this->chunk = this->buffer->getChunk(pos);
      goto do_read;
    }
  do_read:
    std::int16_t *ptr = this->chunk.data + (pos - this->chunk.start) * this->channels;
    std::copy(ptr, ptr + this->channels, out);
  }

  void readFrame(std::size_t pos, float *out) {
    std::array<std::int16_t, config::MAX_CHANNELS> intermediate;
    readFrameI16(pos, &intermediate[0]);
    for (unsigned int i = 0; i < this->channels; i++) {
      out[i] = intermediate[i] / 32768.0f;
    }
  }

  /* Returns frames read. This will be less than requested at the end of the buffer. */
  std::size_t readFramesI16(std::size_t pos, std::size_t count, std::int16_t *out) {
    std::size_t read = 0;
    std::int16_t *cursor = out;

    if (pos > this->length)
      return 0;
    /* can't read more than what's remaining in the buffer. */
    count = std::min(this->length - pos, count);
    while (count - read > 0) {
      std::size_t actual_pos = pos + read;
      /* The overhead of always getting the chunk is minimal, so let's avoid bug-prone branches and always do it. */
      this->chunk = this->buffer->getChunk(actual_pos);
      std::size_t chunk_off = actual_pos - this->chunk.start;
      std::size_t chunk_avail = (this->chunk.end - this->chunk.start) - chunk_off;
      assert(chunk_off + chunk_avail <= this->chunk.end - chunk.start);
      std::size_t will_copy = std::min(chunk_avail, count - read);
      std::int16_t *ptr = this->chunk.data + chunk_off * this->channels;
      std::copy(ptr, ptr + will_copy * channels, cursor);
      cursor += will_copy * this->channels;
      read += will_copy;
    }

    return read;
  }

  /*
   * Read frames into a float*. If the workspace and workspace_len parameters are specified, they will be used as
   * scratch space for the intermediate conversion. They exist to allow for more than the default stack-allocated
   * workspace size, which can speed up the loops here significantly. workspace_len is in elements, not frames.
   * */
  std::size_t readFrames(std::size_t pos, std::size_t count, float *out, std::size_t workspace_len = 0,
                         std::int16_t *workspace = nullptr) {
    std::array<std::int16_t, config::MAX_CHANNELS * 16> default_workspace = {0};

    if (workspace_len * this->channels < default_workspace.size() || workspace == nullptr) {
      workspace = &default_workspace[0];
      workspace_len = default_workspace.size();
    }

    std::size_t workspace_frames = workspace_len / this->channels;
    std::size_t written = 0;
    float *cursor = out;

    while (written < count) {
      std::size_t requested = std::min(count - written, workspace_frames);
      std::size_t got = this->readFramesI16(pos + written, requested, workspace);
      for (unsigned int i = 0; i < got * this->channels; i++) {
        cursor[i] = workspace[i] / 32768.0f;
      }
      cursor += got * this->channels;
      written += got;
      if (got < requested) {
        break;
      }
    }

    return written;
  }

private:
  Buffer *buffer = nullptr;
  /* We hold a copy to avoid going through two pointers and because it's unlikely that compilers can tell this never
   * changes. */
  unsigned int channels = 0;
  std::size_t length = 0;
  BufferChunk chunk;
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
  } catch (...) {
    for (auto x : chunks) {
      deferredFree(x);
    }
    delete resampler;
    deferredFree(next_chunk);
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
