#include <atomic>
#include <cstddef>
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


/*
 * Allocates an output.
 * 
 * Outputs remain docked for the lifetime of the output or unless shutdown is called.
 * */
std::shared_ptr<AudioOutput> createAudioOutput();

}
