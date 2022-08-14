#pragma once

#include "synthizer/cells.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/event_builder.hpp"
#include "synthizer/events.hpp"
#include "synthizer/generation_thread.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include "WDL/resample.h"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace synthizer {

class AudioDecoder;
class Context;
class FadeDriver;

class StreamingGeneratorCommand {
public:
  float *buffer;
  /* If set, seek before doing anything else. */
  std::optional<double> seek;
  /* Final position of the block. */
  double final_position = 0.0;
  /* Used to send events from the main thread. */
  unsigned int finished_count = 0, looped_count = 0;
  /**
   * True if this block was partial, which can happen if the block
   * had an error or was past the end of the stream.
   * */
  bool partial = false;
};

class StreamingGenerator : public Generator {
public:
  StreamingGenerator(const std::shared_ptr<Context> &ctx, const std::shared_ptr<AudioDecoder> &decoder);
  void initInAudioThread() override;
  ~StreamingGenerator();

  int getObjectType() override;
  unsigned int getChannels() override;
  void generateBlock(float *output, FadeDriver *gain_driver) override;

  std::optional<double> startGeneratorLingering() override;

#define PROPERTY_CLASS StreamingGenerator
#define PROPERTY_LIST STREAMING_GENERATOR_PROPERTIES
#define PROPERTY_BASE Generator
#include "synthizer/property_impl.hpp"
private:
  /* The body of the background thread. */
  void generateBlockInBackground(StreamingGeneratorCommand *command);

  GenerationThread<StreamingGeneratorCommand *> background_thread;
  std::shared_ptr<AudioDecoder> decoder = nullptr;
  std::shared_ptr<WDL_Resampler> resampler = nullptr;
  unsigned int channels = 0;
  /* Allocates commands contiguously. */
  deferred_vector<StreamingGeneratorCommand> commands;
  /* Allocates the buffers for the commands contiguously. */
  deferred_vector<float> buffer;
  double background_position = 0.0;
  /* Used to guard against spamming finished events. */
  bool sent_finished = false;
  /**
   * Toggled in the audio thread when lingering begins.
   * */
  bool is_lingering = false;
};

namespace streaming_generator_detail {

constexpr std::size_t STREAMING_GENERATOR_BLOCKS =
    (std::size_t)(nextMultipleOf(0.1 * config::SR, config::BLOCK_SIZE)) / config::BLOCK_SIZE;

struct FillBufferRet {
  double position = 0.0;
  unsigned int looped_count = 0;
  unsigned int finished_count = 0;
  /**
   * True if the block wasn't completely filled.
   * */
  bool partial = false;
};

/*
 * Returns the new position, given the old one.
 *
 * Decoders intentionally don't know how to give us this info, so we have to book keep it ourselves.
 * */
static FillBufferRet fillBufferFromDecoder(AudioDecoder &decoder, unsigned int size, unsigned int channels, float *dest,
                                           bool looping, double position_in) {
  FillBufferRet ret{};
  auto sr = decoder.getSr();
  unsigned int needed = size;
  bool justLooped = false;

  float *cursor = dest;
  ret.position = position_in;
  while (needed > 0) {
    unsigned int got = decoder.writeSamplesInterleaved(needed, cursor);
    cursor += channels * got;
    needed -= got;
    ret.position += got / (double)sr;
    /*
     * justLooped stops us from seeking to the beginning, getting no data, and then looping forever.
     * If we got data, we didn't just loop.
     * 	 */
    justLooped = justLooped && got > 0;
    if (needed > 0 && justLooped == false && looping && decoder.supportsSeek()) {
      ret.looped_count++;
      decoder.seekSeconds(0.0);
      /* We just looped. Keep this set until we get data. */
      justLooped = true;
      ret.position = 0.0;
    } else if (needed > 0) {
      ret.finished_count++;
      break;
    }
  }
  std::fill(cursor, cursor + needed * channels, 0.0f);
  ret.partial = needed != 0;
  return ret;
}
} // namespace streaming_generator_detail

inline StreamingGenerator::StreamingGenerator(const std::shared_ptr<Context> &ctx,
                                              const std::shared_ptr<AudioDecoder> &decoder)
    : Generator(ctx), background_thread(streaming_generator_detail::STREAMING_GENERATOR_BLOCKS), decoder(decoder) {
  this->channels = decoder->getChannels();
  double old_sr = decoder->getSr();
  if (old_sr != config::SR) {
    this->resampler = std::make_shared<WDL_Resampler>();
    /* Configure resampler to use sinc filters and have required sample rates. */
    this->resampler->SetMode(false, 0, true);
    this->resampler->SetRates(old_sr, config::SR);
  }

  this->commands.resize(streaming_generator_detail::STREAMING_GENERATOR_BLOCKS);
  this->buffer.resize(config::BLOCK_SIZE * streaming_generator_detail::STREAMING_GENERATOR_BLOCKS * this->channels,
                      0.0f);
  for (std::size_t i = 0; i < commands.size(); i++) {
    auto &c = commands[i];
    c.buffer = &this->buffer[config::BLOCK_SIZE * this->channels * i];
    this->background_thread.send(&c);
  }

  background_thread.start([this](StreamingGeneratorCommand **item) { this->generateBlockInBackground(*item); });
}

inline void StreamingGenerator::initInAudioThread() {
  Generator::initInAudioThread();
  /*
   * If position starts as changed, StreamingGenerator improperly tries to do an initial seek. This is audible
   * because the background thread runs ahead, and results in an initial audio artifact.
   * */
  this->markPlaybackPositionUnchanged();
}

inline StreamingGenerator::~StreamingGenerator() {
  /* We can't rely on the destructor of background_thread because it runs after ours. */
  this->background_thread.stop();
}

inline int StreamingGenerator::getObjectType() { return SYZ_OTYPE_STREAMING_GENERATOR; }

inline unsigned int StreamingGenerator::getChannels() { return channels; }

inline void StreamingGenerator::generateBlock(float *output, FadeDriver *gd) {
  StreamingGeneratorCommand *cmd;
  double new_pos;

  if (this->background_thread.receive(&cmd) == false) {
    return;
  }

  gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
    for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
      float g = gain_cb(i);
      for (unsigned int ch = 0; ch < this->channels; ch++) {
        output[i * this->channels + ch] += g * cmd->buffer[i * this->channels + ch];
      }
    }
  });

  cmd->seek.reset();
  while (cmd->looped_count > 0) {
    cmd->looped_count--;
    sendLoopedEvent(this->getContext(), this->shared_from_this());
  }
  while (cmd->finished_count > 0) {
    cmd->finished_count--;
    sendFinishedEvent(this->getContext(), this->shared_from_this());
  }
  if (this->acquirePlaybackPosition(new_pos) == true) {
    cmd->seek.emplace(new_pos);
  }
  this->setPlaybackPosition(cmd->final_position, false);

  if (cmd->partial && this->is_lingering) {
    this->signalLingerStopPoint();
  }

  this->background_thread.send(cmd);
}

inline void StreamingGenerator::generateBlockInBackground(StreamingGeneratorCommand *cmd) {
  float *out = cmd->buffer;
  bool looping = this->getLooping();
  streaming_generator_detail::FillBufferRet fill_info;

  try {
    if (cmd->seek && this->decoder->supportsSeek()) {
      this->background_position = cmd->seek.value();
      this->decoder->seekSeconds(this->background_position);
    }
    if (cmd->seek) {
      this->sent_finished = false;
    }

    if (this->resampler == nullptr) {
      std::fill(out, out + config::BLOCK_SIZE * this->getChannels(), 0.0f);
      fill_info = streaming_generator_detail::fillBufferFromDecoder(
          *this->decoder, config::BLOCK_SIZE, this->getChannels(), out, looping, this->background_position);
    } else {
      float *rs_buf;
      int needed = this->resampler->ResamplePrepare(config::BLOCK_SIZE, this->getChannels(), &rs_buf);
      fill_info = streaming_generator_detail::fillBufferFromDecoder(*this->decoder, needed, this->getChannels(), rs_buf,
                                                                    looping, this->background_position);
      unsigned int resampled = this->resampler->ResampleOut(out, needed, config::BLOCK_SIZE, this->getChannels());
      if (resampled < config::BLOCK_SIZE) {
        std::fill(out + resampled * this->getChannels(), out + config::BLOCK_SIZE * this->getChannels(), 0.0f);
      }
    }

    this->background_position = fill_info.position;
    cmd->looped_count = fill_info.looped_count;
    cmd->finished_count = fill_info.finished_count;
    cmd->final_position = this->background_position;
    cmd->partial = fill_info.partial;
    /*
     * Guard against flooding the event queue.
     * */
    if (this->sent_finished == true) {
      cmd->finished_count = 0;
    } else if (cmd->finished_count > 0) {
      this->sent_finished = true;
    }
  } catch (std::exception &e) {
    logError("Background thread for streaming generator had error: %s. Trying to recover...", e.what());
    cmd->partial = true;
  }
}

inline std::optional<double> StreamingGenerator::startGeneratorLingering() {
  /**
   * The streaming generator lingers until it gets  apartial block from the background thread, which
   * either means that the stream finished or errored.  We don't
   * try to recover from errors.
   *
   * We can't always know the duration of streams. When we can, we can't know if they will underrun.  Consequently,
   * we can't specify a linger timeout.
   * */
  setLooping(false);
  this->is_lingering = true;
  return std::nullopt;
}

} // namespace synthizer
