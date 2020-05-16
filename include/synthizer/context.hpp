#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/queues/vyukov.hpp"
#include "synthizer/types.hpp"

#include "sema.h"

#include <atomic>
#include <functional>
#include <thread>

namespace synthizer {

class AudioOutput;

/*
 * The context is the main entrypoint to Synthizer, holding the device, etc.
 * 
 *This has a few responsibilities:
 * - Dispatch callables to a high priority multimedia thread, with tagged priorities.
 * - Hold and orchestrate the lifetime of an audio device.
 * - Hold library-global parameters such as the listener's position and orientation, and configurable default values for things such as the distance model, speed of sound, etc.
 * - Handle memory allocation and freeing as necessary.
 * 
 * Users of synthizer will typically make one context per audio device they wish to use.
 * 
 * Unless otherwise noted, the functions of this class should only be called from the context-managed thread. External callers can submit invokables to run code on that thread, but 
 * since this is audio generation, the context needs full control over the priority of commands.
 * 
 * Later, if necessary, we'll extend synthizer to use atomics for some properties.
 * */
class Context: public BaseObject {
	public:

	Context();
	~Context();

	/*
	 * Shut the context down.
	 * 
	 * This kills the audio thread.
	 * */
	void shutdown();

	/*
	 * Submit an invokable which will be invoked on the context thread.
	 * */
	void enqueueInvokable(Invokable *invokable);

	/* Helper methods used by various pieces of synthizer to grab global resources. */
	/* Allocate a panner lane intended to be used by a source. */
	std::shared_ptr<PannerLane> allocateSourcePannerLane(enum SYZ_PANNER_STRATEGIES strategy);

	/*
	 * add a callback that will get called before every block.
	 * 
	 * This is primarily for testing; once we have a proper source model, we'll move away from it.
	 * */
	template<typename F>
	void addPregenerateCallback(F &&c) {
		this->pregenerate_callbacks.emplace_back(c);
	}

	private:

	/*
	 * Generate a block of audio output for the specified number of channels.
	 * 
	 * The number of channels shouldn't change for the duration of this context in most circumstances.
	 * */
	void generateAudio(unsigned int channels, AudioSample *output);

	/*
	 * The audio thread itself.
	 * */
	void audioThreadFunc();

	VyukovQueue<Invokable> pending_invokables;
	std::thread context_thread;
	/* Wake the context thread, either because a command was submitted or a block of audio was removed. */
	Semaphore context_semaphore;
	std::atomic<int> running;
	std::shared_ptr<AudioOutput> audio_output;

	/* Collections of objects that require execution: sources, etc. all go here eventually. */
	std::shared_ptr<AbstractPannerBank> source_panners = nullptr;

	/* for testing. A list of functions to call before every audio output. */
	std::vector<std::function<void(void)>> pregenerate_callbacks;
};

}