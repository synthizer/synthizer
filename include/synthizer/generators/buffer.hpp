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
  /* Adds to destination, per the generators API. */
  void generateNoPitchBend(float *out, FadeDriver *gain_driver);

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
  double new_pos;

  if (this->handlePropertyConfig() == false) {
    return;
  }

  if (this->acquirePlaybackPosition(new_pos)) {
    this->position_in_samples = std::min(new_pos * config::SR, (double)this->reader.getLengthInSamples(false));
    this->sent_finished = false;
  }

  this->generateNoPitchBend(output, gd);

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

inline void BufferGenerator::generateNoPitchBend(float *output, FadeDriver *gd) {
  std::size_t pos = std::ceil(this->position_in_samples);

  std::size_t will_read_frames = config::BLOCK_SIZE;
  if (pos + will_read_frames > this->reader.getLengthInFrames(false) && this->getLooping() == false) {
    if (pos >= this->reader.getLengthInFrames(false)) {
      // There is nothing to do, since we always add to our output buffer and that's equivalent to adding zeros.
      return;
    }

    will_read_frames = this->reader.getLengthInFrames(false) - pos - 1;
  }

  // Compilers are bad about telling that channels doesn't change.
  unsigned int channels = this->getChannels();

  auto mp = this->reader.getFrameSlice(pos, will_read_frames, false);
  std::visit(
      [&](auto ptr) {
        gd->drive(this->getContextRaw()->getBlockTime(), [&](auto gain_cb) {
          for (std::size_t i = 0; i < will_read_frames; i++) {
            float gain = gain_cb(i) * (1.0f / 32768.0f);
            for (unsigned int ch = 0; ch < channels; ch++) {
              output[i * channels + ch] += ptr[i * channels + ch] * gain;
            }
          }
        });
      },
      mp);

  this->position_in_samples = std::fmod(pos + will_read_frames, this->reader.getLengthInFrames(false));
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
    this->position_in_samples = std::min(new_pos * config::SR, (double)this->reader.getLengthInSamples(false));
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
  double remaining = buf_strong->getLengthInSamples(false) / (double)config::SR - pos;
  if (remaining < 0.0) {
    return 0.0;
  }
  auto pb = this->getPitchBend();
  return remaining / pb;
}

} // namespace synthizer
