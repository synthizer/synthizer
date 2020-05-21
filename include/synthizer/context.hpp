#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/queues/vyukov.hpp"
#include "synthizer/types.hpp"

#include "sema.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <utility>

namespace synthizer {

class AudioOutput;
class Source;

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
class Context: public BaseObject, public std::enable_shared_from_this<Context> {
	public:

	Context();
	~Context();

	std::shared_ptr<Context> getContext() override;

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

	/*
	 * Call a callable in the audio thread.
	 * Convenience method to not have to make invokables everywhere.
	 * */
	template<typename C, typename... ARGS>
	auto call(C &&callable, ARGS&& ...args) {
		auto invokable = WaitableInvokable([&]() {
			return callable(args...);
		});
		this->enqueueInvokable(&invokable);
		return invokable.wait();
	}

	template<typename T, typename... ARGS>
	std::shared_ptr<T> createObject(ARGS&& ...args) {
		auto ret = this->call([&] () {
			return std::make_shared<T>(args...);
		});
		ret->setContext(this->shared_from_this());
		return ret;
	}

	/*
	 * Helpers for the C API. to get/set properties in the context's thread.
	 * These create and manage the invokables and can be called directly.
	 * 
	 * Eventually this will be extended to handle batched/deferred things as well.
	 * */
	int getIntProperty(std::shared_ptr<BaseObject> &obj, int property);
	void setIntProperty(std::shared_ptr<BaseObject> &obj, int property, int value);
	double getDoubleProperty(std::shared_ptr<BaseObject> &obj, int property);
	void setDoubleProperty(std::shared_ptr<BaseObject> &obj, int property, double value);
	std::shared_ptr<BaseObject> getObjectProperty(std::shared_ptr<BaseObject> &obj, int property);
	void setObjectProperty(std::shared_ptr<BaseObject> &obj, int property, std::shared_ptr<BaseObject> &object);

	/*
	 * Ad a weak reference to the specified source.
	 * */
	void registerSource(std::shared_ptr<Source> &source);

	/* Helper methods used by various pieces of synthizer to grab global resources. */
	/* Allocate a panner lane intended to be used by a source. */
	std::shared_ptr<PannerLane> allocateSourcePannerLane(enum SYZ_PANNER_STRATEGIES strategy);

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

	/* The key is a raw pointer for easy lookup. */
	std::unordered_map<void *, std::weak_ptr<Source>> sources;
	std::shared_ptr<AbstractPannerBank> source_panners = nullptr;
};

}