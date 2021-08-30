#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>

#include "WDL/resample.h"
#include "miniaudio.h"

#include "synthizer/audio_output.hpp"
#include "synthizer/config.hpp"
#include "synthizer/error.hpp"
#include "synthizer/logging.hpp"

namespace synthizer {
class AudioOutputDevice;

class AudioOutputImpl : public AudioOutput {
public:
  AudioOutputImpl(const std::function<void(unsigned int, float *)> &callback);
  ~AudioOutputImpl();
  void fillBuffer(float *buffer);
  void shutdown();
  std::weak_ptr<AudioOutputDevice> device;
  std::weak_ptr<AudioOutputImpl> self;
  std::function<void(unsigned int, float *)> callback;
};

/*
 * Audio devices are one per physical output device, and manipulated through global functions.
 *
 * This is so that we don't have to open multiple copies of the same device, and also simplifies moving between devices
 * if necessary.
 *
 * */
class AudioOutputDevice {
public:
  AudioOutputDevice();
  ~AudioOutputDevice();

  void doOutput(std::size_t frames, float *destination);
  void refillWorkingBuffer();

  float *working_buffer;
  // in frames.
  std::size_t working_buffer_remaining = 0;

  Dock<AudioOutputImpl> dock;
  WDL_Resampler resampler;
  ma_device_config device_config;
  ma_device device;
};

static void miniaudioDataCallback(ma_device *device, void *output, const void *input, ma_uint32 frames) {
  (void)input;

  AudioOutputDevice *dev = (AudioOutputDevice *)device->pUserData;
  dev->doOutput(frames, (float *)output);
}

AudioOutputDevice::AudioOutputDevice() {
  this->working_buffer = new float[config::BLOCK_SIZE * 2];
  auto &config = this->device_config;
  config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.periodSizeInFrames = 0;
  /* Default periods. */
  config.periods = 0;
  /* Use default sample rate. */
  config.sampleRate = 0;
  config.dataCallback = miniaudioDataCallback;
  config.pUserData = (void *)this;
  config.performanceProfile = ma_performance_profile_low_latency;

  if (ma_device_init(NULL, &config, &this->device) != MA_SUCCESS)
    throw EAudioDevice("Unable to initialize miniaudio.");

  /* Configure resampler to use sinc filters and have required sample rates. */
  this->resampler.SetMode(false, 0, true);
  this->resampler.SetRates(config::SR, this->device.sampleRate);

  if (ma_device_start(&this->device) != MA_SUCCESS) {
    ma_device_uninit(&this->device);
    throw EAudioDevice("Unable to start Miniaudio device");
  }
}

AudioOutputDevice::~AudioOutputDevice() {
  ma_device_uninit(&this->device);
  delete[] this->working_buffer;
}

void AudioOutputDevice::refillWorkingBuffer() {
  // For now all devices are stereo only, but this is where we'd implement the final mixing later.
  std::fill(this->working_buffer, this->working_buffer + config::BLOCK_SIZE * 2, 0.0f);
  this->dock.walk([&](auto &dev) { dev.fillBuffer(this->working_buffer); });
  this->working_buffer_remaining = config::BLOCK_SIZE;
}

void AudioOutputDevice::doOutput(std::size_t frames, float *destination) {
  float *resample_buffer;
  int needed_frames = this->resampler.ResamplePrepare(frames, 2, (WDL_ResampleSample **)&resample_buffer);

  float *cur = resample_buffer;
  int remaining_frames = needed_frames;
  while (remaining_frames) {
    if (this->working_buffer_remaining == 0)
      this->refillWorkingBuffer();

    auto to_copy = std::min<std::size_t>(remaining_frames, this->working_buffer_remaining);
    float *wb = this->working_buffer + 2 * config::BLOCK_SIZE - 2 * this->working_buffer_remaining;
    std::copy(wb, wb + to_copy * 2, cur);
    cur += to_copy * 2;
    this->working_buffer_remaining -= to_copy;
    remaining_frames -= to_copy;
  }

  /* If this outputs less, miniaudio already zeroed for us. */
  this->resampler.ResampleOut((WDL_ResampleSample *)destination, needed_frames, frames, 2);
}

static std::shared_ptr<AudioOutputDevice> output_device = nullptr;

void initializeAudioOutputDevice() { output_device = std::make_shared<AudioOutputDevice>(); }

void shutdownOutputDevice() {
  if (output_device == nullptr)
    throw EUninitialized();
  output_device = nullptr;
}

AudioOutputImpl::AudioOutputImpl(const std::function<void(unsigned int, float *)> &callback) : callback(callback) {}

AudioOutputImpl::~AudioOutputImpl() { shutdown(); }

void AudioOutputImpl::fillBuffer(float *buffer) { this->callback(2, buffer); }

void AudioOutputImpl::shutdown() {
  auto dev = this->device.lock();
  if (dev == nullptr)
    return;
  auto us = this->self.lock();
  dev->dock.undock(us);
}

std::shared_ptr<AudioOutput> createAudioOutput(const std::function<void(unsigned int, float *)> &callback) {
  if (output_device == nullptr) {
    throw EUninitialized();
  }

  auto ao = std::make_shared<AudioOutputImpl>(callback);
  ao->self = ao;
  ao->device = output_device;
  output_device->dock.dock(ao);
  return ao;
}

} // namespace synthizer