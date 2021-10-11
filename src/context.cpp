#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/property_xmacros.hpp"

#include "synthizer/at_scope_exit.hpp"
#include "synthizer/audio_output.hpp"
#include "synthizer/background_thread.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/property_automation_timeline.hpp"
#include "synthizer/sources.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include "sema.h"
#include <concurrentqueue.h>

#include <cmath>
#include <functional>
#include <memory>
#include <utility>

namespace synthizer {

Context::Context() : BaseObject(nullptr) {}

void Context::initContext(bool is_headless) {
  std::weak_ptr<Context> ctx_weak = this->getContext();

  this->source_panners = createPannerBank();

  this->headless = is_headless;
  if (headless) {
    this->delete_directly.store(1);
    return;
  }

  this->audio_output = createAudioOutput([ctx_weak](unsigned int channels, float *buffer) {
    auto ctx_strong = ctx_weak.lock();
    if (ctx_strong == nullptr) {
      return;
    }
    ctx_strong->generateAudio(channels, buffer);
  });
  this->running.store(1);
}

Context::~Context() {
  if (this->running.load()) {
    this->shutdown();
  }
  this->drainDeletionQueues();
}

std::shared_ptr<Context> Context::getContext() { return std::static_pointer_cast<Context>(this->shared_from_this()); }

int Context::getObjectType() { return SYZ_OTYPE_CONTEXT; }

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
  this->command_queue.processAll([](auto &cmd) { cmd.deinitialize(); });
}

void Context::cDelete() {
  logDebug("C deleted context");
  if (this->running.load())
    this->shutdown();
}

template <typename T>
void Context::propertySetter(const std::shared_ptr<BaseObject> &obj, int property, const T &value) {
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

void Context::setBiquadProperty(std::shared_ptr<BaseObject> &obj, int property, const struct syz_BiquadConfig &value) {
  this->propertySetter<syz_BiquadConfig>(obj, property, value);
}

void Context::registerSource(const std::shared_ptr<Source> &source) {
  /* We can capture this because, in order to invoke the command, we have to still have the context around. */
  this->enqueueReferencingCallbackCommand(
      true, [this](auto &src) { this->sources[src.get()] = src; }, source);
}

void Context::registerGlobalEffect(const std::shared_ptr<GlobalEffect> &effect) {
  /* We can capture this because, in order to invoke the command, we have to still have the context around. */
  this->enqueueReferencingCallbackCommand(
      true, [this](auto &effect) { this->global_effects.push_back(effect); }, effect);
}

std::shared_ptr<PannerLane> Context::allocateSourcePannerLane(enum SYZ_PANNER_STRATEGY strategy) {
  return this->source_panners->allocateLane(strategy);
}

void Context::generateAudio(unsigned int channels, float *destination) {
  if (this->running.load() == 0 && this->headless == false) {
    return;
  }

  this->in_audio_callback.store(1);
  auto release_audio_thread = AtScopeExit([&]() { this->in_audio_callback.store(0); });

  /* No exception should happen without programmer error, but if it does we want to fail loudly. */
  try {
    this->runCommands();

    // And now that we've done that, run our own automation, which doesn't run otherwise.
    this->tickAutomation();

    DeletionRecord rec;
    while (pending_deletes.try_dequeue(rec)) {
      rec.callback(rec.arg);
    }

    std::fill(destination, destination + channels * config::BLOCK_SIZE, 0.0f);

    /**
     * This is the first safe place to actually pause: the output is definitely playing silence, and all commands
     * have executed. While we can pause the context, and pausing the context pauses everything undernneath, pausing
     * the queues will cause unrecoverable deadlocks because it will be impossible to unpause and the queues will
     * eventually fill.
     * */
    if (this->isPaused()) {
      return;
    }

    std::fill(this->getDirectBuffer(), this->getDirectBuffer() + config::BLOCK_SIZE * channels, 0.0f);

    /**
     * Do all the automation first, then run the sources.
     * */
    {
      auto i = this->sources.begin();
      while (i != this->sources.end()) {
        auto v = i->second;
        auto s = v.lock();
        if (s == nullptr) {
          i = this->sources.erase(i);
          continue;
        }
        s->tickAutomation();
        i++;
      }
    }

    /**
     * We just handled deleting all the soures; this time, we don't need to worry about that.
     * */
    for (auto s : this->sources) {
      s.second.lock()->run();
    }

    this->source_panners->run(channels, destination);

    weak_vector::iterate_removing(this->global_effects, [&](auto &e) { e->run(channels, this->getDirectBuffer()); });
    this->getRouter()->finishBlock();

    /* Write the direct buffer. */
    for (unsigned int j = 0; j < config::BLOCK_SIZE * channels; j++) {
      destination[j] += this->direct_buffer[j];
    }

    /**
     * Handle gain. Note that destination was zeroed and only contains audio from this invocation.
     *
     * This must come after commands, which might change the property.
     * */
    double new_gain;
    if (this->acquireGain(new_gain) || this->shouldIncorporatePausableGain()) {
      new_gain *= this->getPausableGain();
      this->gain_driver.setValue(this->getBlockTime(), new_gain);
    }
    /**
     * Can tick the pausable here.
     * */
    this->tickPausable();

    this->gain_driver.drive(this->getBlockTime(), [&](auto &gain_cb) {
      for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
        float g = gain_cb(i);
        for (unsigned int ch = 0; ch < channels; ch++) {
          unsigned int ind = i * channels + ch;
          destination[ind] *= g;
        }
      }
    });

    this->lingering_objects.popUntilPriority(this->getBlockTime(), [](auto prio, auto &obj) {
      (void)prio;
      auto strong = obj.lock();
      if (strong) {
        strong->dieNow();
      }
    });
    this->lingering_objects.filterAllItems([](auto prio, auto &obj) {
      (void)prio;
      return obj.expired();
    });

    this->block_time.fetch_add(1, std::memory_order_relaxed);
  } catch (...) {
    logError("Got an exception in the audio callback");
  }
}

void Context::runCommands() {
  this->command_queue.processAll([&](auto &cmd) {
    try {
      auto deinit = AtScopeExit([&]() { cmd.deinitialize(); });
      cmd.execute();
    } catch (std::exception &e) {
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
  while (this->deletes_in_progress.load(std::memory_order_acquire) != 0)
    ;

  DeletionRecord rec;
  while (this->pending_deletes.try_dequeue(rec)) {
    rec.callback(rec.arg);
  }
}

void Context::enableEvents() { this->event_sender.setEnabled(true); }

void Context::getNextEvent(syz_Event *out) { this->event_sender.getNextEvent(out); }

void Context::doLinger(const std::shared_ptr<BaseObject> &obj) {
  if (obj->wantsLinger() == false) {
    return;
  }

  auto delete_cfg = obj->getDeleteBehaviorConfig();
  if (delete_cfg.linger == false) {
    return;
  }

  this->enqueueCallbackCommand([this, obj, delete_cfg]() {
    auto maybe_suggested_timeout = obj->startLingering(obj, delete_cfg.linger_timeout);
    if (!maybe_suggested_timeout && delete_cfg.linger_timeout <= 0.0) {
      /* The object will manage ending the linger itself. */
      return;
    }
    double suggested_timeout = maybe_suggested_timeout.value_or(delete_cfg.linger_timeout);
    /**
     * If objects are allowed to raise the linger timeout, the user's timeout is not respected. This isn't what we want:
     * users should always be able to know when their objects die.
     * */
    double actual_timeout = suggested_timeout;
    if (delete_cfg.linger_timeout > 0.0) {
      actual_timeout = std::min(suggested_timeout, delete_cfg.linger_timeout);
    }

    /**
     * If the linger timeout is 0.0, then let's kill the object now.
     *
     * This is important because some objects will return 0.0 when they can't work out their
     * timeouts until `startLingering` is called in the audio thread.
     * This works correctly without the optimization, but at the cost of spamming the linger queue and keeping the
     * object alive for a further block.
     * */
    if (actual_timeout == 0.0) {
      obj->dieNow();
      return;
    }

    double block_timeout = std::ceil(actual_timeout * config::SR / config::BLOCK_SIZE);
    std::uint64_t block_time = this->getBlockTime() + block_timeout;
    /**
     * Then schedule the delete.
     * */
    this->lingering_objects.push(block_time, obj);
  });
}

void Context::enqueueLingerStop(const std::shared_ptr<BaseObject> &obj) {
  this->lingering_objects.push(0, std::static_pointer_cast<CExposable>(obj));
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createContext(syz_Handle *out, void *userdata,
                                         syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  auto *ctx = new Context();
  std::shared_ptr<Context> ptr{ctx, deleteInBackground<Context>};
  ptr->initContext();
  auto ce = std::static_pointer_cast<CExposable>(ptr);
  ce->stashInternalReference(ce);
  *out = toC(ptr);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createContextHeadless(syz_Handle *out, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  auto *ctx = new Context();
  std::shared_ptr<Context> ptr{ctx, deleteInBackground<Context>};
  /* This is a headless context. */
  ptr->initContext(true);
  auto ce = std::static_pointer_cast<CExposable>(ptr);
  ce->stashInternalReference(ce);
  *out = toC(ptr);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_contextGetBlock(syz_Handle context, float *block) {
  SYZ_PROLOGUE
  auto ctx = fromC<Context>(context);
  ctx->generateAudio(2, block);
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_contextEnableEvents(syz_Handle context) {
  SYZ_PROLOGUE
  std::shared_ptr<Context> ctx = fromC<Context>(context);
  ctx->enqueueReferencingCallbackCommand(
      true, [](auto &ctx) { ctx->enableEvents(); }, ctx);
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_contextGetNextEvent(struct syz_Event *out, syz_Handle context, unsigned long long flags) {
  (void)flags;

  SYZ_PROLOGUE
  std::shared_ptr<Context> ctx = fromC<Context>(context);
  ctx->getNextEvent(out);
  return 0;
  SYZ_EPILOGUE
}
