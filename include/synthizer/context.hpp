#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/queues/vyukov.hpp"
#include "synthizer/types.hpp"

#include "sema.h"

#include <thread>

namespace synthizer {

/*
 * The context is the main entrypoint to Synthizer, holding the device, etc.
 * 
 *This has a few responsibilities:
 * - Dispatch callables to a high priority multimedia threadk, with tagged priorities.
 * - Hold and orchestrate the lifetime of an audio device.
 * - Hold library-global parameters such as the listener's position and orientation, and configurable default values for things such as the distance model, speed of sound, etc.
 * - Handle memory allocation and freeing as necessary.
 * 
 * Users of synthizer will typically make one context per audio device they wish to use.
 * 
 * Unless otherwise noted, the functions of this class should only be called from the context-managed thread. External callers can submit invokables to run code on that thread, but 
 * since this is audio generation, the context needs full control over the priority of commands.
 * */
class Context: public BaseObject {
	public:

	/*
	 * Generate a block of audio output for the specified number of channels.
	 * 
	 * The number of channels shouldn't change for the duration of this context in most circumstances.
	 * */
	void run(unsigned int channels, AudioSample *output);

	/*
	 * Submit an invokable which will be invoked on the context thread.
	 * */
	void submitInvokable(Invokable *invokable);

	/*
	 * Request that the context generate audio as soon as possible.
	 * 
	 * Can be called from any thread.
	 * */
	void enqueueAudioGeneration();

	/* Helper methods used by various pieces of synthizer to grab global resources. */
	/* Allocate a panner lane intended to be used by a source. */
	std::shared_ptr<AbstractPannerLane> allocateSourcePannerLane(enum SYZ_PANNER_STRATEGIES strategy;

	private:
	VyukovQueue<Invokable> pending_invokables;
	std::thread context_thread;
	/* Wake the context thread, either because a command was submitted or a block of audio was removed. */
	Semaphore context_semaphore;

	/* Collections of objects that require execution: sources, etc. all go here eventually. */
	std::shared_ptr<AbstractPannerBank> source_panners = nullptr;
};

}