#pragma once

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/buffer.hpp"
#include "synthizer/context.hpp"
#include "synthizer/edge_trigger.hpp"
#include "synthizer/events.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vbool.hpp"

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
  /**
   * Adds to the buffer. Returns by how much to increment the position.
   * */
  std::size_t generateNoPitchBend(float *out, FadeDriver *gain_driver);

  /**
   * Adds to the buffer. Returns by how much to increment the position.
   * */
  std::size_t generatePitchBend(float *out, FadeDriver *gain_driver);

  /*
   * Handle configuring properties, and set the non-property state variables up appropriately.
   *
   * Returns true if processing of the block should proceed, or false if there is no buffer and processing of the block
   * should be skipped.
   */
  bool handlePropertyConfig();

  BufferReader reader;

  /**
   * Set when this generator finishes. Used as an edge trigger to send a finished event, since buffers can only finish
   * once per block.
   * */
  bool finished = false;

  std::size_t position_in_frames = 0;

  /**
   * The fractional part of the position used for handling pitch bend.
   * */
  double pitch_fraction = 0.0;
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
    this->position_in_frames =
        std::min<std::size_t>(new_pos * config::SR, (double)this->reader.getLengthInSamples(false));
    this->finished = false;
  }

  // We saw the end and haven't seeked or set the buffer, so don't do anything.
  if (this->finished == true) {
    return;
  }

  std::size_t pos_increment;

  if (this->getPitchBend() == 1.0) {
    pos_increment = this->generateNoPitchBend(output, gd);
  } else {
    pos_increment = this->generatePitchBend(output, gd);
  }

  if (this->getLooping()) {
    unsigned int loop_count = (this->position_in_frames + pos_increment + 1) / this->reader.getLengthInFrames(false);
    for (unsigned int i = 0; i < loop_count; i++) {
      sendLoopedEvent(this->getContext(), this->shared_from_this());
    }
  } else if (this->finished == false &&
             this->position_in_frames + pos_increment + 1 >= this->reader.getLengthInFrames(false)) {
    sendFinishedEvent(this->getContext(), this->shared_from_this());
    this->finished = true;
  }

  this->position_in_frames = (this->position_in_frames + pos_increment) % this->reader.getLengthInFrames(false);
  this->setPlaybackPosition(this->position_in_frames / (double)config::SR, false);
}

inline std::size_t BufferGenerator::generateNoPitchBend(float *output, FadeDriver *gd) {
  assert(this->finished == false);

  std::size_t will_read_frames = config::BLOCK_SIZE;
  if (this->position_in_frames + will_read_frames > this->reader.getLengthInFrames(false) &&
      this->getLooping() == false) {
    will_read_frames = this->reader.getLengthInFrames(false) - this->position_in_frames - 1;
    will_read_frames = std::min<std::size_t>(will_read_frames, config::BLOCK_SIZE);
  }

  // Compilers are bad about telling that channels doesn't change.
  const unsigned int channels = this->getChannels();

  auto mp = this->reader.getFrameSlice(this->position_in_frames, will_read_frames, false, true);
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

  return will_read_frames;
}

inline std::size_t BufferGenerator::generatePitchBend(float *output, FadeDriver *gd) {
  assert(this->finished == false);

  double delta = this->getPitchBend();
  std::size_t wrote_frames;

  // This is a bit complicated.  First, we will read up to 1 more than the block size * delta.  But if that's past the
  // end, we will instead read up to the end, and truncate our pitch bend early.
  //
  // We might also read a little bit more if pitch_fraction is big enough.
  //
  // This computation doesn't include the implicit zero, though we do use that, below.
  std::size_t read_span_original = std::ceil(config::BLOCK_SIZE * delta + this->pitch_fraction);
  std::size_t read_span = read_span_original;
  if (this->position_in_frames + read_span > this->reader.getLengthInFrames(false) && this->getLooping() == false) {
    read_span = this->reader.getLengthInFrames(false) - this->position_in_frames - 1;
  }

  // Compilers are bad about telling that channels doesn't change.
  const unsigned int channels = this->getChannels();

  // if we aren't looping, we include the implicit zero so that we can go slightly past the end of the buffer if needed,
  // but need to tell getFrameSlice that we'll read that as well as that we want it.
  //
  // If we are looping, then we don't cut things off at upper and will need to read one more sample.
  auto mp = this->reader.getFrameSlice(this->position_in_frames, read_span + 1, this->getLooping() == false, true);

  bool truncated_non_vbool = read_span != read_span_original;

  // In the below, it is very important never to add or subtract positions and to always use i * delta.  This avoids
  // accumulating fp error.
  std::visit(
      [&](auto ptr,
          auto truncated // whether or not we can output BLOCK_SIZE samples.
      ) {
        gd->drive(this->getContextRaw()->getBlockTime(), [&](auto gain_cb) {
          // we need these two at the end, but leave upper in the loop to help the compiler realize that it doesn't
          // introduce loop dependencies.
          std::size_t i = 0, lower = 0;

          for (; i < config::BLOCK_SIZE; i++) {
            // We need to use doubles, because 65535 (the sum of two samples) is outside the range of integers a float
            // can represent.
            double gain = gain_cb(i) * (1.0 / 32768.0);

            lower = delta * i;
            std::size_t upper = lower + 1;

            if (lower >= read_span && truncated == true) {
              break;
            }

            double w2 = delta * i - lower;
            double w1 = 1.0f - w2;

            // Work these in here and we can avoid doing them in the loop that handles channels individually.
            w1 *= gain;
            w2 *= gain;

            for (unsigned int ch = 0; ch < channels; ch++) {
              float l_s = ptr[lower * channels + ch], u_s = ptr[upper * channels + ch];
              float o = l_s * w1 + u_s * w2;
              output[i * channels + ch] += o;
            }

            wrote_frames++;
          }

          // If this was truncated and upper is not at or past the end, then we have one more sample at the end of the
          // buffer that is worth writing this time.
          if (truncated == true && (this->position_in_frames + lower) < this->reader.getLengthInFrames(false) - 1 &&
              i + 1 < config::BLOCK_SIZE) {
            std::size_t final_frame_start = this->reader.getLengthInFrames(false) - channels;
            float gain = gain_cb(i) / 32768.0f;
            for (std::size_t ch = 0; ch < channels; ch++) {
              output[i + ch] += ptr[final_frame_start + ch] * gain;
            }

            wrote_frames++;
          }
        });
      },
      mp, vCond(truncated_non_vbool));

  this->pitch_fraction = std::fmod(this->position_in_frames + delta * wrote_frames, 1.0);

  // The caller BufferGenerator::generate can handle going past the end of the buffer in the case when we're not
  // looping, and when we're looping will_read_frames == will_read_frames_original.
  return read_span_original;
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
  this->finished = false;

  // It is possible that the user set the buffer then changed the playback position.  It is very difficult to tell the
  // difference between this and setting the position immediately before changing the buffer without rewriting the
  // entire property infrastructure so, under the assumption that the common case is trying to set both together we
  // (sometimes) will treat these cases the same if they happen in the audio tick.
  //
  // Hopefully, this is rare.
  if (this->acquirePlaybackPosition(new_pos)) {
    this->position_in_frames = std::min(new_pos * config::SR, (double)this->reader.getLengthInSamples(false));
  } else {
    this->setPlaybackPosition(0.0, false);
    this->position_in_frames = 0.0;
  }

  return false;
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
