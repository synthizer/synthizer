#include "synthizer.h"
#include "synthizer_constants.h"
#include "synthizer_properties.h"

#include "synthizer/audio_output.hpp"
#include "synthizer/background_thread.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/property_ring.hpp"
#include "synthizer/sources.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include "concurrentqueue.h"
#include "sema.h"

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
void Context::propertySetter(const std::shared_ptr<BaseObject> &obj, int property, T &value) {
	obj->validateProperty(property, value);
	if (this->property_ring.enqueue(obj, property, value)) {
		return;
	}

	auto inv = WaitableInvokable([&] () {
		/*
		 * The context will flush, then we do the set here. Next time, the queue's empty.
		 * */
		obj->setProperty(property, value);
	});
	this->enqueueInvokable(&inv);
	inv.wait();
}

int Context::getIntProperty(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<int>(this, obj, property);
}

void Context::setIntProperty(std::shared_ptr<BaseObject> &obj, int property, int value) {
	this->propertySetter<int>(obj, property, value);
}

double Context::getDoubleProperty(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<double>(this, obj, property);
}

void Context::setDoubleProperty(std::shared_ptr<BaseObject> &obj, int property, double value) {
	this->propertySetter<double>(obj, property, value);
}

std::shared_ptr<CExposable> Context::getObjectProperty(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<std::shared_ptr<CExposable>>(this, obj, property);
}

void Context::setObjectProperty(std::shared_ptr<BaseObject> &obj, int property, std::shared_ptr<CExposable> &value) {
	this->propertySetter<std::shared_ptr<CExposable>>(obj, property, value);
}

std::array<double, 3> Context::getDouble3Property(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<std::array<double, 3>>(this, obj, property);
}

void Context::setDouble3Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 3> value) {
	this->propertySetter<std::array<double, 3>>(obj, property, value);
}

std::array<double, 6>  Context::getDouble6Property(std::shared_ptr<BaseObject> &obj, int property) {
	return propertyGetter<std::array<double, 6>>(this, obj, property);
}

void Context::setDouble6Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 6> value) {
	this->propertySetter<std::array<double, 6>>(obj, property, value);
}

void Context::registerSource(const std::shared_ptr<Source> &source) {
	this->call([&] () {
		this->sources[source.get()] = source;
	});
}

void Context::registerGlobalEffect(const std::shared_ptr<GlobalEffectBase> &effect) {
	this->call([&] () {
		this->global_effects.push_back(effect);
	});
}

std::array<double, 3> Context::getPosition() {
	return this->position;
}

void Context::setPosition(std::array<double, 3> pos) {
	this->position = pos;
}

std::array<double, 6> Context::getOrientation() {
	return this->orientation;
}

void Context::setOrientation(std::array<double, 6> orientation) {
	Vec3d at{ orientation[0], orientation[1], orientation[2] };
	Vec3d up{ orientation[3], orientation[4], orientation[5] };
	throwIfParallel(at, up);
	this->orientation = orientation;
}

std::shared_ptr<PannerLane> Context::allocateSourcePannerLane(enum SYZ_PANNER_STRATEGY strategy) {
	return this->source_panners->allocateLane(strategy);
}

void Context::generateAudio(unsigned int channels, AudioSample *destination) {
	std::fill(destination, destination + channels * config::BLOCK_SIZE, 0.0f);
	std::fill(this->getDirectBuffer(), this->getDirectBuffer() + config::BLOCK_SIZE * channels, 0.0f);

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

	weak_vector::iterate_removing(this->global_effects, [&](auto &e) {
		e->run(channels, this->getDirectBuffer());
	});
	this->getRouter()->finishBlock();

	/* Write the direct buffer. */
	for (unsigned int i = 0; i < config::BLOCK_SIZE * channels; i++) {
		destination[i] += this->direct_buffer[i];
	}
}

void Context::flushPropertyWrites() {
	while (1) {
		try {
			if (this->property_ring.applyNext() == false) {
				break;
			}
		} catch(std::exception &e) {
			logError("Got exception applying property write: %s", e.what());
		}
	}
}

void Context::audioThreadFunc() {
	logDebug("Thread started");
	while (this->running.load()) {
		AudioSample *write_audio = nullptr;

		this->flushPropertyWrites();

		write_audio = this->audio_output->beginWrite();
		while (write_audio) {
			this->generateAudio(2, write_audio);
			this->audio_output->endWrite();
			write_audio = this->audio_output->beginWrite();
		}

		Invokable *inv;
		while (pending_invokables.try_dequeue(inv)) {
			this->flushPropertyWrites();
			inv->invoke();
		}

		/*
		 * This needs to be moved to a dedicated thread. eventually, but this will do for now.
		 * Unfortunately doing better is going to require migrating off shared_ptr, though it's not clear yet as to what it will be replaced with.
		 * */
		DeletionRecord rec;
		while (pending_deletes.try_dequeue(rec)) {
			rec.callback(rec.arg);
		}

		this->context_semaphore.wait();
	}

	this->audio_output->shutdown();
	logDebug("Context thread terminating");
}

void Context::enqueueDeletionRecord(DeletionCallback cb, void *arg) {
	DeletionRecord rec;


	this->deletes_in_progress.fetch_add(1, std::memory_order_relaxed);
	rec.callback = cb;
	rec.arg = arg;
	this->pending_deletes.enqueue(rec);
	this->deletes_in_progress.fetch_sub(1, std::memory_order_release);
}

void Context::drainDeletionQueues() {
	while(this->deletes_in_progress.load(std::memory_order_acquire) != 0);

	DeletionRecord rec;
	while (this->pending_deletes.try_dequeue(rec)) {
		rec.callback(rec.arg);
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

#define PROPERTY_CLASS Context
#define PROPERTY_BASE BaseObject
#define PROPERTY_LIST CONTEXT_PROPERTIES
#include "synthizer/property_impl.hpp"
