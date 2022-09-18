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

#include <cstdint>
#include <memory>
#include <optional>

namespace synthizer {

/**
 * Plays a buffer.
 *
 * It is worth taking a moment to notate what is going on with positions and pitch bend.  In order to implement precise
 * pitch bend, this class uses a scaled position that can do what is in effect fixed point math (see
 * config::BUFFER_POS_MULTIPLIER). This allows us to avoid fp error on that path, though is mildly inconvenient
 * everywhere else.
 * */
class BufferGenerator : public Generator {
public:
  BufferGenerator(std::shared_ptr<Context> ctx);

  int getObjectType() override;
  unsigned int getChannels() const override;
  void generateBlock(float *output, FadeDriver *gain_driver) override;
  void seek(double new_pos);
  std::uint64_t getPosInSamples() const;

  std::optional<double> startGeneratorLingering() override;

#define PROPERTY_CLASS BufferGenerator
#define PROPERTY_BASE Generator
#define PROPERTY_LIST BUFFER_GENERATOR_PROPERTIES
#include "synthizer/property_impl.hpp"

private:
  void generateNoPitchBend(float *out, FadeDriver *gain_driver);
  void generatePitchBend(float *out, FadeDriver *gain_driver) const;

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

  std::uint64_t scaled_position_in_frames = 0, scaled_position_increment = 0;
};

inline BufferGenerator::BufferGenerator(std::shared_ptr<Context> ctx) : Generator(ctx) {}

inline int BufferGenerator::getObjectType() { return SYZ_OTYPE_BUFFER_GENERATOR; }

inline unsigned int BufferGenerator::getChannels() const {
  auto buf_weak = this->getBuffer();

  auto buffer = buf_weak.lock();
  if (buffer == nullptr)
    return 0;
  return buffer->getChannels();
}

inline void BufferGenerator::seek(double new_pos) {
  std::uint64_t new_pos_samples = new_pos * config::SR;
  new_pos_samples = new_pos_samples >= this->reader.getLengthInFrames(false) ? this->reader.getLengthInFrames(false) - 1
                                                                             : new_pos_samples;
  this->scaled_position_in_frames = new_pos_samples * config::BUFFER_POS_MULTIPLIER;
  this->finished = false;
  this->setPlaybackPosition(this->getPosInSamples() / (double)config::SR, false);
}

inline std::uint64_t BufferGenerator::getPosInSamples() const {
  return this->scaled_position_in_frames / config::BUFFER_POS_MULTIPLIER;
}

inline void BufferGenerator::generateBlock(float *output, FadeDriver *gd) {
  double new_pos;

  if (this->handlePropertyConfig() == false) {
    return;
  }

  if (this->acquirePlaybackPosition(new_pos)) {
    this->seek(new_pos);
  }

  // We saw the end and haven't seeked or set the buffer, so don't do anything.
  if (this->finished == true) {
    return;
  }

  // it is possible for the generator to need to advance less if it is at or near the end, but we deal withv that below
  // and avoid very complicated computations that try to work out what it actually is: we did that in the past, and it
  // lead to no end of bugs.
  this->scaled_position_increment = config::BUFFER_POS_MULTIPLIER * this->getPitchBend();
  std::uint64_t scaled_pos_increment = this->scaled_position_increment * config::BLOCK_SIZE;

  if (this->getPitchBend() == 1.0) {
    this->generateNoPitchBend(output, gd);
  } else {
    this->generatePitchBend(output, gd);
  }

  if (this->getLooping()) {
    // If we are looping, then the position can always go past the end.
    unsigned int loop_count = (this->scaled_position_in_frames + scaled_pos_increment + config::BUFFER_POS_MULTIPLIER) /
                              (config::BUFFER_POS_MULTIPLIER * this->reader.getLengthInFrames(false));
    for (unsigned int i = 0; i < loop_count; i++) {
      sendLoopedEvent(this->getContext(), this->shared_from_this());
    }
    this->scaled_position_in_frames = (this->scaled_position_in_frames + scaled_pos_increment) %
                                      (this->reader.getLengthInFrames(false) * config::BUFFER_POS_MULTIPLIER);
  } else if (this->finished == false &&
             this->scaled_position_in_frames + scaled_pos_increment + config::BUFFER_POS_MULTIPLIER >=
                 this->reader.getLengthInFrames(false) * config::BUFFER_POS_MULTIPLIER) {
    // In this case, the position might be past the end so we'll set it to the end exactly when we're done.
    sendFinishedEvent(this->getContext(), this->shared_from_this());
    this->finished = true;
    this->scaled_position_in_frames = this->reader.getLengthInFrames(false) * config::BUFFER_POS_MULTIPLIER;
  } else {
    // this won't need modulus, because otherwise that would have meant looping or ending.
    this->scaled_position_in_frames += scaled_pos_increment;
  }

  this->setPlaybackPosition(this->getPosInSamples() / (double)config::SR, false);
}

inline void BufferGenerator::generateNoPitchBend(float *output, FadeDriver *gd) {
  assert(this->finished == false);

  // Bump scaled_position_in_frames up to the next multiplier, if necessary.
  // this->scaled_position_in_frames = ceilByPowerOfTwo(this->scaled_position_in_frames, config::BUFFER_POS_MULTIPLIER);

  std::size_t will_read_frames = config::BLOCK_SIZE;
  if (this->getPosInSamples() + will_read_frames > this->reader.getLengthInFrames(false) &&
      this->getLooping() == false) {
    will_read_frames = this->reader.getLengthInFrames(false) - this->getPosInSamples() - 1;
    will_read_frames = std::min<std::size_t>(will_read_frames, config::BLOCK_SIZE);
  }

  // Compilers are bad about telling that channels doesn't change.
  const unsigned int channels = this->getChannels();

  auto mp = this->reader.getFrameSlice(this->scaled_position_in_frames / config::BUFFER_POS_MULTIPLIER,
                                       will_read_frames, false, true);
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
}

namespace buffer_generator_detail {

/**
 * Parameters needed to do pitch bend.
 *
 * If pitch bend can't be done because delta is 0, this is zero-initialized and iterations = 0.
 * */
struct PitchBendParams {
  std::uint64_t offset;
  std::size_t iterations;

  /**
   * The span we must grab from the underlying buffer, in samples.
   *
   * Possibly includes the implicit zero, if required.
   * */
  std::size_t span_start, span_len;

  /**
   * Whether or not the buffer should include the implicit zero.
   * */
  bool include_implicit_zero;
};

PitchBendParams computePitchBendParams(std::uint64_t scaled_position, std::uint64_t delta,
                                       std::uint64_t buffer_len_no_zero, bool looping) {
  PitchBendParams ret{};

  // We have some fractional offset of the position, which we need to use to know how far from the first sample of this
  // slice over the buffer is.
  const std::uint64_t offset = scaled_position % config::BUFFER_POS_MULTIPLIER;

  // If delta is 0 then computing our iterations can divide by zero, and it'd be 0 movement anyway.
  if (delta == 0) {
    ret.iterations = 0;
    return ret;
  }

  // This is a bit complicated.  First, we will read up to 1 more than the block size * delta.  But if that's past the
  // end, we will instead read up to the end, and truncate our pitch bend early.
  //
  // Don't forget that the fractional part of the position contributes.
  std::uint64_t scaled_read_span_tmp = offset + config::BLOCK_SIZE * delta;
  // the actual bound is the scaled span's ceil.  If this is exactly equal to the scaled span, we must go one more
  // sample: even though the below loop would use a 0 inbterpolation weight, it's stil going to try to read one after
  // the end.
  std::uint64_t scaled_read_span = ceilByPowerOfTwo(scaled_read_span_tmp, config::BUFFER_POS_MULTIPLIER);
  scaled_read_span =
      (scaled_read_span == scaled_read_span_tmp) ? scaled_read_span + config::BUFFER_POS_MULTIPLIER : scaled_read_span;
  // Note that in the case where we had to bump scaled_read_span up by a sample, it's already an exact multiplier, so
  // floor is fine.
  std::uint64_t read_span = scaled_read_span / config::BUFFER_POS_MULTIPLIER;

  std::size_t loop_iterations = config::BLOCK_SIZE;

  // If the spamn is going to read past the end and we aren't looping, we must bring it down.
  if (looping == false) {
    // Recall that the span can read into the implicit zero without issue.
    if (scaled_position / config::BUFFER_POS_MULTIPLIER + read_span >= buffer_len_no_zero + 1) {
      // When figuring out what's available, we must not include the implicit zero.  That is, available is the maximum
      // movement of the lower sample in the interpolation.
      std::uint64_t available = buffer_len_no_zero - scaled_position / config::BUFFER_POS_MULTIPLIER - 1;
      // We know read_span was going to read past the end, and thus that available is less than that. Available
      // doesn't include the implicit zero, so we add it back.
      read_span = available + 1;
      // But the total number of iterations is based only on scaling available, in this case.
      loop_iterations = available * config::BUFFER_POS_MULTIPLIER / delta;
    }
  }

  ret.include_implicit_zero = looping == false;
  ret.iterations = loop_iterations;
  ret.offset = offset;
  ret.span_start = scaled_position / config::BUFFER_POS_MULTIPLIER;
  ret.span_len = read_span;
  return ret;
}
} // namespace buffer_generator_detail

inline void BufferGenerator::generatePitchBend(float *output, FadeDriver *gd) const {
  assert(this->finished == false);

  const auto params =
      buffer_generator_detail::computePitchBendParams(this->scaled_position_in_frames, this->scaled_position_increment,
                                                      this->reader.getLengthInFrames(false), this->getLooping());

  if (params.iterations == 0) {
    return;
  }

  auto mp = this->reader.getFrameSlice(params.span_start, params.span_len, params.include_implicit_zero, true);

  // In the case where the number of iterations is the block size, we can use VBool to let the compiler know.
  bool _is_full_block = params.iterations == config::BLOCK_SIZE;

  // the compiler is bad about telling that channels doens't change.
  const unsigned int channels = this->getChannels();

  std::visit(
      [&](auto ptr, auto full_block) {
        gd->drive(this->getContextRaw()->getBlockTime(), [&](auto gain_cb) {
          // Let the compiler understand that, sometimes, it can fully unroll the loop.
          const std::size_t iters = full_block ? config::BLOCK_SIZE : params.iterations;
          const std::uint64_t delta = this->scaled_position_increment;

          for (std::size_t i = 0; i < iters; i++) {
            std::uint64_t scaled_effective_pos = params.offset + delta * i;
            std::size_t scaled_lower = floorByPowerOfTwo(scaled_effective_pos, config::BUFFER_POS_MULTIPLIER);
            std::size_t lower = scaled_lower / config::BUFFER_POS_MULTIPLIER;

            // We are close enough to the point where floats start erroring that we probably want doubles, or at least
            // to do some sort of actual error analysis which hasn't been done as of this writing. Thus, doubles for
            // now.
            //
            // Also factor in the conversion from 16-bit signed samples into these weights.
            double w2 = (scaled_effective_pos - scaled_lower) * (1.0 / config::BUFFER_POS_MULTIPLIER);
            double w1 = 1.0 - w2;
            w1 *= (1.0 / 32768.0);
            w2 *= (1.0 / 32768.0);
            float gain = gain_cb(i);

            for (unsigned int ch = 0; ch < channels; ch++) {
              std::int16_t l = ptr[lower * channels + ch], u = ptr[(lower + 1) * channels + ch];
              output[i * channels + ch] = gain * (w1 * l + w2 * u);
            }
          }
        });
      },
      mp, vCond(_is_full_block));
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

  // It is possible that the user set the buffer then changed the playback position.  It is very difficult to tell the
  // difference between this and setting the position immediately before changing the buffer without rewriting the
  // entire property infrastructure so, under the assumption that the common case is trying to set both together we
  // (sometimes) will treat these cases the same if they happen in the audio tick.
  //
  // Hopefully, this is rare.
  if (this->acquirePlaybackPosition(new_pos)) {
    this->seek(new_pos);
  } else {
    this->seek(0.0);
  }

  return buffer != nullptr;
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
