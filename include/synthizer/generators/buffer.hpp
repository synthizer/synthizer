#pragma once

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/buffer.hpp"
#include "synthizer/context.hpp"
#include "synthizer/edge_trigger.hpp"
#include "synthizer/events.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include <memory>
#include <optional>

namespace synthizer {

class FadeDriver;

class BufferGenerator : public Generator {
public:
  BufferGenerator(std::shared_ptr<Context> ctx);

  int getObjectType() override;
  unsigned int getChannels() override;
  void generateBlock(float *output, FadeDriver *gain_driver) override;

  std::optional<double> startGeneratorLingering() override;

#define PROPERTY_CLASS BufferGenerator
#define PROPERTY_BASE Generator
#define PROPERTY_LIST BUFFER_GENERATOR_PROPERTIES
#include "synthizer/property_impl.hpp"

private:
  template <bool L> void readInterpolated(double pos, float *out, float gain);

  /* Adds to destination, per the generators API. */
  void generateNoPitchBend(float *out, FadeDriver *gain_driver);

  /**
   * In the common case we know that we can get from the buffer in one read. This covers that case.
   * */
  void generateByCopying(float *out, FadeDriver *gain_driver);
  template <bool L> void generatePitchBendHelper(float *out, FadeDriver *gain_driver, double pitch_bend);

  void generatePitchBend(float *out, FadeDriver *gain_driver, double pitch_bend);

  /*
   * Handle configuring properties, and set the non-property state variables up appropriately.
   *
   * Returns true if processing of the block should proceed, or false if there is no buffer and processing of the block
   * should be skipped.
   */
  bool handlePropertyConfig();
  /**
   * Either sends finished or looped, depending.
   * */
  void handleEndEvent();

  BufferReader reader;
  /**
   * Counters for events.
   * */
  unsigned int finished_count = 0, looped_count = 0;
  bool sent_finished = false;
  double position_in_samples = 0.0;
};

inline BufferGenerator::BufferGenerator(std::shared_ptr<Context> ctx) : Generator(ctx) {}

inline int BufferGenerator::getObjectType() { return SYZ_OTYPE_BUFFER_GENERATOR; }

inline unsigned int BufferGenerator::getChannels() {
  auto buf_weak = this->getBuffer();

  auto buffer = buf_weak.lock();
  if (buffer == nullptr)
    return 0;
  return buffer->getChannels();
}

inline void BufferGenerator::generateBlock(float *output, FadeDriver *gd) {
  double new_pos, pitch_bend;

  if (this->handlePropertyConfig() == false) {
    return;
  }

  pitch_bend = this->getPitchBend();

  if (this->acquirePlaybackPosition(new_pos)) {
    this->position_in_samples = std::min(new_pos * config::SR, (double)this->reader.getLength());
    this->sent_finished = false;
  }

  if (std::fabs(1.0 - pitch_bend) > 0.001) {
    this->generatePitchBend(output, gd, pitch_bend);
  } else {
    this->generateNoPitchBend(output, gd);
  }

  this->setPlaybackPosition(this->position_in_samples / config::SR, false);

  while (this->looped_count > 0) {
    sendLoopedEvent(this->getContext(), this->shared_from_this());
    this->looped_count--;
  }
  while (this->finished_count > 0) {
    sendFinishedEvent(this->getContext(), this->shared_from_this());
    this->finished_count--;
  }
}

template <bool L> inline void BufferGenerator::readInterpolated(double pos, float *out, float gain) {
  std::array<float, config::MAX_CHANNELS> f1, f2;
  std::size_t lower = std::floor(pos);
  std::size_t upper = lower + 1;
  if (L)
    upper = upper % this->reader.getLength();
  // Important: upper < lower if upper was past the last sample.
  float w2 = pos - lower;
  float w1 = 1.0 - w2;
  this->reader.readFrame(lower, &f1[0]);
  this->reader.readFrame(upper, &f2[0]);
  for (unsigned int i = 0; i < this->reader.getChannels(); i++) {
    out[i] += gain * (f1[i] * w1 + f2[i] * w2);
  }
}

template <bool L>
inline void BufferGenerator::generatePitchBendHelper(float *output, FadeDriver *gd, double pitch_bend) {
  double pos = this->position_in_samples;
  double delta = pitch_bend;
  gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      float g = gain_cb(i);
      this->readInterpolated<L>(pos, &output[i * this->reader.getChannels()], g);
      double new_pos = pos + delta;
      if (L == true) {
        new_pos = std::fmod(new_pos, this->reader.getLength());
        if (new_pos < pos) {
          this->looped_count += 1;
        }
      } else if (pos > this->reader.getLength()) {
        if (this->sent_finished == false) {
          /* Don't forget to guard against sending this multiple times. */
          this->finished_count += 1;
          this->sent_finished = true;
        }
        break;
      }
      pos = new_pos;
    }
  });
  this->position_in_samples = std::min<double>(pos, this->reader.getLength());
}

inline void BufferGenerator::generatePitchBend(float *output, FadeDriver *gd, double pitch_bend) {
  if (this->getLooping()) {
    return this->generatePitchBendHelper<true>(output, gd, pitch_bend);
  } else {
    return this->generatePitchBendHelper<false>(output, gd, pitch_bend);
  }
}

inline void BufferGenerator::generateByCopying(float *output, FadeDriver *gd) {
  std::size_t pos = std::round(this->position_in_samples);

  // the compiler is not smart enough to tell that the value of getChannels never changes, so we need to pull it to a
  // variable.
  unsigned int channels = this->getChannels();

  gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
    const std::int16_t *raw = this->reader.getRawSlice(pos * channels, (pos + config::BLOCK_SIZE) * channels);

    for (unsigned int i = 0; i < config::BLOCK_SIZE * channels; i++) {
      output[i] += raw[i] * (1.0f / 32768.0f) * gain_cb(i / channels);
    }
  });

  this->position_in_samples = pos + config::BLOCK_SIZE;
}

inline void BufferGenerator::generateNoPitchBend(float *output, FadeDriver *gd) {
  std::size_t pos = std::round(this->position_in_samples);

  // We can sometimes delegate to the generateByCopy optimized path.  Do so when we aren't going to hit the end.  We
  // could move the end event handling there and fully optimize buffers which are exactly the block size, but that's not
  // worth the complexity for now.
  if (pos + config::BLOCK_SIZE + 1 < this->reader.getLength() / this->reader.getChannels()) {
    return this->generateByCopying(output, gd);
  }

  float *cursor = output;
  unsigned int remaining = config::BLOCK_SIZE;
  auto workspace_guard = acquireBlockBuffer();
  float *workspace = workspace_guard;
  bool looping = this->getLooping() != 0;
  unsigned int i = 0;

  // the compiler is bad about telling that the channel count never changes, so lift it to a variable.
  unsigned int channels = this->getChannels();

  gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
    while (remaining) {
      auto got = this->reader.readFrames(pos, remaining, workspace);
      for (unsigned int j = 0; j < got; i++, j++) {
        float g = gain_cb(i);
        for (unsigned int ch = 0; ch < channels; ch++) {
          cursor[j * channels + ch] += g * workspace[j * channels + ch];
        }
      }
      remaining -= got;
      cursor += got * channels;
      pos += got;
      if (remaining > 0) {
        if (looping == false) {
          if (this->sent_finished == false) {
            this->finished_count += 1;
            this->sent_finished = true;
          }
          break;
        } else {
          pos = 0;
          this->looped_count += 1;
        }
      }
    }
  });
  assert(i <= config::BLOCK_SIZE);

  this->position_in_samples = pos;
}

inline bool BufferGenerator::handlePropertyConfig() {
  std::weak_ptr<Buffer> buffer_weak;
  std::shared_ptr<Buffer> buffer;
  bool buffer_changed = this->acquireBuffer(buffer_weak);
  double new_pos;
  buffer = buffer_weak.lock();

  if (buffer_changed == false) {
    // Just tell the caller if there's a buffer.
    return buffer != nullptr;
  }

  this->reader.setBuffer(buffer.get());
  this->sent_finished = false;

  // It is possible that the user set the buffer then changed the playback position.  It is very difficult to tell the
  // difference between this and setting the position immediately before changing the buffer without rewriting the
  // entire property infrastructure so, under the assumption that the common case is trying to set both together we
  // (sometimes) will treat these cases the same if they happen in the audio tick.
  //
  // Hopefully, this is rare.
  if (this->acquirePlaybackPosition(new_pos)) {
    this->position_in_samples = std::min(new_pos * config::SR, (double)this->reader.getLength());
  } else {
    this->setPlaybackPosition(0.0, false);
    this->position_in_samples = 0.0;
  }

  return false;
}

inline void BufferGenerator::handleEndEvent() {
  auto ctx = this->getContext();
  if (this->getLooping() == 1) {
    sendLoopedEvent(ctx, this->shared_from_this());
  } else {
    sendFinishedEvent(ctx, this->shared_from_this());
  }
}

inline std::optional<double> BufferGenerator::startGeneratorLingering() {
  /**
   * To linger, stop any looping, then set the timeout to the duration of the buffer
   * minus the current position.
   * */
  double pos = this->getPlaybackPosition();
  this->setLooping(false);
  auto buf = this->getBuffer();
  auto buf_strong = buf.lock();
  if (buf_strong == nullptr) {
    return 0.0;
  }
  double remaining = buf_strong->getLength() / (double)config::SR - pos;
  if (remaining < 0.0) {
    return 0.0;
  }
  auto pb = this->getPitchBend();
  return remaining / pb;
}

} // namespace synthizer
