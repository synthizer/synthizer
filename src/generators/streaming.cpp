#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/generators/streaming.hpp"

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/cells.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/events.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/stream_handle.hpp"

#include "WDL/resample.h"

#include <algorithm>
#include <memory>
#include <optional>

namespace synthizer {

constexpr std::size_t STREAMING_GENERATOR_BLOCKS =
    (std::size_t)(nextMultipleOf(0.1 * config::SR, config::BLOCK_SIZE)) / config::BLOCK_SIZE;

StreamingGenerator::StreamingGenerator(const std::shared_ptr<Context> &ctx,
                                       const std::shared_ptr<AudioDecoder> &decoder)
    : Generator(ctx), background_thread(STREAMING_GENERATOR_BLOCKS), decoder(decoder) {
  this->channels = decoder->getChannels();
  double old_sr = decoder->getSr();
  if (old_sr != config::SR) {
    this->resampler = std::make_shared<WDL_Resampler>();
    /* Configure resampler to use sinc filters and have required sample rates. */
    this->resampler->SetMode(false, 0, true);
    this->resampler->SetRates(old_sr, config::SR);
  }

  this->commands.resize(STREAMING_GENERATOR_BLOCKS);
  this->buffer.resize(config::BLOCK_SIZE * STREAMING_GENERATOR_BLOCKS * this->channels, 0.0f);
  for (std::size_t i = 0; i < commands.size(); i++) {
    auto &c = commands[i];
    c.buffer = &this->buffer[config::BLOCK_SIZE * this->channels * i];
    this->background_thread.send(&c);
  }

  background_thread.start([this](StreamingGeneratorCommand **item) { this->generateBlockInBackground(*item); });
}

void StreamingGenerator::initInAudioThread() {
  Generator::initInAudioThread();
  /*
   * If position starts as changed, StreamingGenerator improperly tries to do an initial seek. This is audible
   * because the background thread runs ahead, and results in an initial audio artifact.
   * */
  this->markPlaybackPositionUnchanged();
}

StreamingGenerator::~StreamingGenerator() {
  /* We can't rely on the destructor of background_thread because it runs after ours. */
  this->background_thread.stop();
}

int StreamingGenerator::getObjectType() { return SYZ_OTYPE_STREAMING_GENERATOR; }

unsigned int StreamingGenerator::getChannels() { return channels; }

void StreamingGenerator::generateBlock(float *output, FadeDriver *gd) {
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

void StreamingGenerator::generateBlockInBackground(StreamingGeneratorCommand *cmd) {
  float *out = cmd->buffer;
  bool looping = this->getLooping();
  FillBufferRet fill_info;

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
      fill_info = fillBufferFromDecoder(*this->decoder, config::BLOCK_SIZE, this->getChannels(), out, looping,
                                        this->background_position);
    } else {
      float *rs_buf;
      int needed = this->resampler->ResamplePrepare(config::BLOCK_SIZE, this->getChannels(), &rs_buf);
      fill_info = fillBufferFromDecoder(*this->decoder, needed, this->getChannels(), rs_buf, looping,
                                        this->background_position);
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

std::optional<double> StreamingGenerator::startGeneratorLingering() {
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

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamParams(syz_Handle *out, syz_Handle context,
                                                                    const char *protocol, const char *path, void *param,
                                                                    void *config, void *userdata,
                                                                    syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto decoder = getDecoderForStreamParams(protocol, path, param);
  auto generator = ctx->createObject<StreamingGenerator>(decoder);
  *out = toC(generator);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromFile(syz_Handle *out, syz_Handle context, const char *path,
                                                            void *config, void *userdata,
                                                            syz_UserdataFreeCallback *userdata_free_callback) {
  return syz_createStreamingGeneratorFromStreamParams(out, context, "file", path, NULL, config, userdata,
                                                      userdata_free_callback);
}

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamHandle(syz_Handle *out, syz_Handle context,
                                                                    syz_Handle stream, void *config, void *userdata,
                                                                    syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto s = fromC<StreamHandle>(stream);
  auto bs = consumeStreamHandle(s);
  auto decoder = getDecoderForStream(bs);
  auto generator = ctx->createObject<StreamingGenerator>(decoder);
  *out = toC(generator);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
