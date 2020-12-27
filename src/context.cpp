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

Context::Context(): Pausable(nullptr) { }

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
	/* We're shut down, just deinitialize all of them to kill strong references. */
	this->command_queue.processAll([] (auto &cmd) {
		cmd.deinitialize();
	});
}

void Context::cDelete() {
	logDebug("C deleted context");
	if (this->running.load()) this->shutdown();
}

template<typename T>
void Context::propertySetter(const std::shared_ptr<BaseObject> &obj, int property, T &value) {
	obj->validateProperty(property, value);
	this->enqueueReferencingCallbackCommand(true, setPropertyCmd, property, obj, property_impl::PropertyValue(value));
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
	/* We can capture this because, in order to invoke the command, we have to still have the context around. */
	this->enqueueReferencingCallbackCommand(true, [this] (auto &src) {
		this->sources[src.get()] = src;
	}, source);
}

void Context::registerGlobalEffect(const std::shared_ptr<GlobalEffect> &effect) {
	/* We can capture this because, in order to invoke the command, we have to still have the context around. */
	this->enqueueReferencingCallbackCommand(true, [this] (auto &effect) {
		this->global_effects.push_back(effect);
	}, effect);
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

/* No exception should happen without programmer error, but if it does we want to fail loudly. */
	try {
		this->runCommands();

		DeletionRecord rec;
		while (pending_deletes.try_dequeue(rec)) {
			rec.callback(rec.arg);
		}


		std::fill(destination, destination + channels * config::BLOCK_SIZE, 0.0f);

		/**
		 * This is the first safe place to actually pause: the output is definitely playing silence, and all commands have executed.
		 * While we can pause the context, and pausing the context pauses everything undernneath, pausing the queues will cause unrecoverable deadlocks because it will be impossible
		 * to unpause and the queues will eventually fill.
		 * */
		if (this->isPaused()) {
			return;
		}
	
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

		/**
		 * Handle gain. Note that destination was zeroed and only contains audio from this invocation, so the final step is to
		 * 
		 * This must come after commands, which might change the property.
		 * */
		double new_gain;
		if (this->acquireGain(new_gain) || this->shouldIncorporatePausableGain()) {
			new_gain *= this->getPausableGain();
			this->gain_driver.setValue(this->block_time, new_gain);
		}
		/**
		 * Can tick the pausable here.
		 * */
		this->tickPausable();

		this->gain_driver.drive(this->block_time, [&](auto &gain_cb) {
			for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
				float g = gain_cb(i);
				for (unsigned int ch = 0; ch < channels; ch++) {
					unsigned int ind = i * channels + ch;
					destination[ind] *= g;
				}
			}
		});

		this->block_time++;
	} catch(...) {
		logError("Got an exception in the audio callback");
	}
}

void Context::runCommands() {
	this->command_queue.processAll([&](auto &cmd) {
		try {
			auto deinit = AtScopeExit([&] () { cmd.deinitialize(); });
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
