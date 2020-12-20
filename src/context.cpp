#include "synthizer.h"
#include "synthizer_constants.h"
#include "synthizer/property_xmacros.hpp"

#include "synthizer/audio_output.hpp"
#include "synthizer/at_scope_exit.hpp"
#include "synthizer/background_thread.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/logging.hpp"
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

void Context::initContext( bool headless) {
	std::weak_ptr<Context> ctx_weak = this->shared_from_this();

	this->source_panners = createPannerBank();

	this->headless = headless;
	if (headless) {
		this->delete_directly.store(1);
		return;
	}

	this->audio_output = createAudioOutput([ctx_weak] (unsigned int channels, float *buffer) {
		auto ctx_strong = ctx_weak.lock();
		ctx_strong->generateAudio(channels, buffer);
	});
	this->running.store(1);
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
	if (this->headless == false) {
		logDebug("Context shutdown");
		this->running.store(0);
		this->audio_output->shutdown();
		/*
		* We want to make sure that the audio callback has seen our shutdown. Otherwise, it may try to run the loop.
		* After this, the audio callback will output 0 until such time as the context shared_ptr dies.
		* */
		while (this->in_audio_callback.load()) {
			std::this_thread::yield();
		}
		this->delete_directly.store(1);
	}
	this->drainDeletionQueues();
}

void Context::cDelete() {
	logDebug("C deleted context");
	if (this->running.load()) this->shutdown();
}

void Context::enqueueInvokable(Invokable *invokable) {
	pending_invokables.enqueue(invokable);
}

template<typename T>
void Context::propertySetter(const std::shared_ptr<BaseObject> &obj, int property, T &value) {
	obj->validateProperty(property, value);
	this->enqueueCallableCommand(setPropertyCmd, property, obj, property_impl::PropertyValue(value));
}

void Context::setIntProperty(std::shared_ptr<BaseObject> &obj, int property, int value) {
	this->propertySetter<int>(obj, property, value);
}

void Context::setDoubleProperty(std::shared_ptr<BaseObject> &obj, int property, double value) {
	this->propertySetter<double>(obj, property, value);
}

void Context::setObjectProperty(std::shared_ptr<BaseObject> &obj, int property, std::shared_ptr<CExposable> &value) {
	this->propertySetter<std::shared_ptr<CExposable>>(obj, property, value);
}

void Context::setDouble3Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 3> value) {
	this->propertySetter<std::array<double, 3>>(obj, property, value);
}

void Context::setDouble6Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 6> value) {
	this->propertySetter<std::array<double, 6>>(obj, property, value);
}

void Context::registerSource(const std::shared_ptr<Source> &source) {
	this->call([&] () {
		this->sources[source.get()] = source;
	});
}

void Context::registerGlobalEffect(const std::shared_ptr<GlobalEffect> &effect) {
	this->call([&] () {
		this->global_effects.push_back(effect);
	});
}

std::shared_ptr<PannerLane> Context::allocateSourcePannerLane(enum SYZ_PANNER_STRATEGY strategy) {
	return this->source_panners->allocateLane(strategy);
}

void Context::generateAudio(unsigned int channels, float *destination) {
	if (this->running.load() == 0 && this->headless == false) {
		return;
	}

	this->in_audio_callback.store(1);
	auto release_audio_thread = AtScopeExit([&]() {
		this->in_audio_callback.store(0);
	});

	/*
	 * no exception should ever be thrown, but if that proves not to be the case we want to know about it.
	 * */
	try {
		this->runCommands();

		Invokable *inv;
		while (pending_invokables.try_dequeue(inv)) {
			this->runCommands();
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

		this->block_time++;
	} catch(...) {
		logError("Got an exception in the audio callback");
	}
}

void Context::runCommands() {
	this->command_queue.processAll([&](auto &cmd) {
		try {
			cmd.execute();
		} catch(std::exception &e) {
			logError("Got exception applying property write: %s", e.what());
		}
	});
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

SYZ_CAPI syz_ErrorCode syz_createContextHeadless(syz_Handle *out) {
	SYZ_PROLOGUE
	auto *ctx = new Context();
	std::shared_ptr<Context> ptr{ctx, deleteInBackground<Context>};
	/* This is a headless context. */
	ptr->initContext(true);
	*out = toC(ptr);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_contextGetBlock(syz_Handle context, float *block) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	ctx->generateAudio(2, block);
	return 0;
	SYZ_EPILOGUE
}
