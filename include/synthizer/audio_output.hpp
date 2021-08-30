#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>

#include "config.hpp"
#include "dock.hpp"
#include "error.hpp"

namespace synthizer {

class AudioOutputDevice;

class AudioOutput {
public:
  virtual ~AudioOutput() {}
  virtual void shutdown() = 0;
};

ERRDEF(EAudioDevice, "Audio device error");

/*
 * Call this once. Opens audio devices.
 *
 * Throws a standard error exception on error to be translated by the C API.
 * */
void initializeAudioOutputDevice();
void shutdownOutputDevice();

/*
 * Allocates an output.
 *
 * Outputs remain docked for the lifetime of the output or unless shutdown is called.
 *
 * The argument is a callback (channels, output) which should fill output with 1 block of data by adding, as is the
 * Synthizer convention.
 * */
std::shared_ptr<AudioOutput> createAudioOutput(const std::function<void(unsigned int, float *)> &callback);

} // namespace synthizer
