#include "synthizer.h"

#include "synthizer/background_thread.hpp"
#include "synthizer/context.hpp"

#include "synthizer/audio_output.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/sources.hpp"
#include "synthizer/types.hpp"

#include <functional>
#include <memory>
#include <utility>

namespace synthizer {

Context::Context(): BaseObject(nullptr) { }

void Context::initContext() {
	std::weak_ptr<Context> ctx_weak = this->shared_from_this();
	this->audio_output = createAudioOutput([ctx_weak] () {
		auto ctx_strong = ctx_weak.lock();
		if (ctx_strong) ctx_strong->context_semaphore.signal();
	}, false);

	this->source_panners = createPannerBank();
	this->running.store(1);
	this->context_thread = std::move(std::thread([&] () {
		setThreadPurpose("context-thread");
		this->audioThreadFunc();
	}));
}

Context::~Context() {
	if(this->running.load()) {
		this->shutdown();
	}
	this->drainDeletionQueues();
}

std::shared_ptr<Context> Context::getContext() {
	return this->shared_from_this();
}

void Context::shutdown() {
	logDebug("Context shutdown");
	this->running.store(0);
	this->context_semaphore.signal();
	this->context_thread.join();

	this->delete_directly.store(1);
	this->drainDeletionQueues();
}

void Context::cDelete() {
	logDebug("C deleted context");
	if (this->running.load()) this->shutdown();
}

void Context::enqueueInvokable(Invokable *invokable) {
	pending_invokables.enqueue(invokable);
	this->context_semaphore.signal();
}

template<typename T>
static T propertyGetter(Context *ctx, std::shared_ptr<BaseObject> &obj, int property) {
	auto inv = WaitableInvokable([&] () {
		return std::get<T>(obj->getProperty(property));
	});
	ctx->enqueueInvokable(&inv);
	return inv.wait();
}

template<typename T>
static void propertySetter(Context *ctx, std::shared_ptr<BaseObject> &obj, int property, T &value) {
	auto inv = WaitableInvokable([&] () {
		obj->setProperty(property, value);
	});
	ctx->enqueueInvokable(&inv);
	inv.wait();
}

int Context::getIntProperty(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<int>(this, obj, property);
}

void Context::setIntProperty(std::shared_ptr<BaseObject> &obj, int property, int value) {
	propertySetter<int>(this, obj, property, value);
}

double Context::getDoubleProperty(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<double>(this, obj, property);
}

void Context::setDoubleProperty(std::shared_ptr<BaseObject> &obj, int property, double value) {
	propertySetter<double>(this, obj, property, value);
}

std::shared_ptr<BaseObject> Context::getObjectProperty(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<std::shared_ptr<BaseObject>>(this, obj, property);
}

void Context::setObjectProperty(std::shared_ptr<BaseObject> &obj, int property, std::shared_ptr<BaseObject> &value) {
	propertySetter<std::shared_ptr<BaseObject>>(this, obj, property, value);
}

void Context::registerSource(std::shared_ptr<Source> &source) {
	this->sources[source.get()] = source;
}

std::shared_ptr<PannerLane> Context::allocateSourcePannerLane(enum SYZ_PANNER_STRATEGIES strategy) {
	return this->source_panners->allocateLane(strategy);
}

void Context::generateAudio(unsigned int channels, AudioSample *destination) {
	std::fill(destination, destination + channels * config::BLOCK_SIZE, 0.0f);

	auto i = this->sources.begin();
	while (i != this->sources.end()) {
		auto [k, v] = *i;
		auto s = v.lock();
		if (s == nullptr) {
			i = this->sources.erase(i);
			continue;
		}
		s->run();
		i++;
	}

	this->source_panners->run(channels, destination);
}

void Context::audioThreadFunc() {
	logDebug("Thread started");
	while (this->running.load()) {
		AudioSample *write_audio = nullptr;

		write_audio = this->audio_output->beginWrite();
		while (write_audio) {
			this->generateAudio(2, write_audio);
			this->audio_output->endWrite();
			write_audio = this->audio_output->beginWrite();
		}

		Invokable *inv = this->pending_invokables.dequeue();
		while (inv) {
			inv->invoke();
			inv = this->pending_invokables.dequeue();
		}

		/*
		 * This needs to be moved to a dedicated thread. eventually, but this will do for now.
		 * Unfortunately doing better is going to require migrating off shared_ptr, though it's not clear yet as to what it will be replaced with.
		 * */
		DeletionRecord *rec;
		while ((rec = this->pending_deletes.dequeue())) {
			rec->callback(rec->arg);
			this->free_deletes.enqueue(rec);
		}

		this->context_semaphore.wait();
	}

	this->audio_output->shutdown();
	logDebug("Context thread terminating");
}

void Context::enqueueDeletionRecord(DeletionCallback cb, void *arg) {
	DeletionRecord *rec;

	try {
		this->deletes_in_progress.fetch_add(1, std::memory_order_relaxed);

		{
			std::lock_guard g(this->free_deletes_mutex);
			rec = this->free_deletes.dequeue();
		}
		if (rec == nullptr) {
			rec = new DeletionRecord();
		}
		rec->callback = cb;
		rec->arg = arg;
		this->pending_deletes.enqueue(rec);
		this->deletes_in_progress.fetch_sub(1, std::memory_order_release);
	} catch(...) {
		/* Swallow exceptions; we don't want to stop deletes. */
	}
	this->deletes_in_progress.fetch_sub(1, std::memory_order_release);
}

void Context::drainDeletionQueues() {
	while(this->deletes_in_progress.load(std::memory_order_acquire) != 0);

	DeletionRecord *rec;
	while ((rec = this->pending_deletes.dequeue())) {
		rec->callback(rec->arg);
		delete rec;
	}
	while ((rec = this->free_deletes.dequeue())) {
		delete rec;
	}
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createContext(syz_Handle *out) {
	SYZ_PROLOGUE
	auto *ctx = new Context();
	std::shared_ptr<Context> ptr{ctx, deleteInBackground<Context>};
	ptr->initContext();
	*out = toC(ptr);
	return 0;
	SYZ_EPILOGUE
}
