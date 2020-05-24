#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>

#include "audio_ring.hpp"
#include "config.hpp"
#include "dock.hpp"
#include "error.hpp"
#include "queues/bounded_block_queue.hpp"

namespace synthizer {

class AudioOutputDevice;

class AudioOutput {
	public:
	virtual ~AudioOutput() {}
	virtual AudioSample *beginWrite() = 0;
	virtual void endWrite() = 0;
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
 * The argument is a callback which will be called on underflow and/or when a buffer is available in the queue.
 * */
std::shared_ptr<AudioOutput> createAudioOutput(std::function<void(void)> availability_callback = {}, bool blocking = true);

}
