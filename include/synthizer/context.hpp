#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/commands.hpp"
#include "synthizer/config.hpp"
#include "synthizer/events.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/mpsc_ring.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/pausable.hpp"
#include "synthizer/priority_queue.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/router.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"

#include <concurrentqueue.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

struct syz_Event;

namespace synthizer {

class AudioOutput;
class CExposable;
class ExposedAutomationTimeline;
class Source;
class GlobalEffect;

/*
 * Infrastructure for deletion.
 * */
template <std::size_t ALIGN> void contextDeferredFreeCallback(void *p) {
  if (ALIGN <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
    ::operator delete(p);
  } else {
    ::operator delete(p, (std::align_val_t)ALIGN);
  }
}

template <typename T> void deletionCallback(void *p) {
  T *y = (T *)p;
  y->~T();
  deferredFreeCallback(contextDeferredFreeCallback<alignof(T)>, (void *)y);
}

/*
 * The context is the main entrypoint to Synthizer, holding the device, etc.
 *
 *This has a few responsibilities:
 * - Dispatch callables to a high priority multimedia thread, with tagged priorities.
 * - Hold and orchestrate the lifetime of an audio device.
 * - Hold library-global parameters such as the listener's position and orientation, and configurable default values for
 *things such as the distance model, speed of sound, etc.
 * - Handle memory allocation and freeing as necessary.
 *
 * Users of synthizer will typically make one context per audio device they wish to use.
 *
 * Unless otherwise noted, the functions of this class should only be called from the context-managed thread.
 * */
class Context : public Pausable, public BaseObject {
public:
  Context();
  /*
   * Initialization occurs in two phases. The constructor does almost nothing, then this is called.
   *
   * Why is because it is unfortunately necessary for the audio thread from miniaudio to hold a weak_ptr, which needs us
   * to be able to use shared_from_this.
   * */
  void initContext(bool headless = false);

  ~Context();

  std::shared_ptr<Context> getContext() override;
  Context *getContextRaw() override { return this; }

  int getObjectType() override;

  /*
   * Shut the context down.
   *
   * This kills the audio thread.
   * */
  void shutdown();
  void cDelete() override;

  /**
   * Call a callable in the audio thread. Doesn't wait for completion. Returns false
   * if there was no room in the queue.
   * */
  template <typename CB, typename... ARGS> bool enqueueCallbackCommandNonblocking(CB &&callback, ARGS &&... args) {
    if (this->headless) {
      callback(args...);
      return true;
    }

    /* If the context isn't running, it's shut down. Currently, contexts can't be restarted, and we don't fully support
     * headless contexts yet. */
    if (this->running.load(std::memory_order_relaxed) == 0) {
      return true;
    }

    return this->command_queue.write([&](auto &cmd) { initCallbackCommand(&cmd, callback, args...); });
  }

  /**
   * Like enqueueCallbackCommandNonblocking, but spins.
   *
   * In practice, code goes through this one instead, and we rely on knowing that there's a reasonable size for the
   * command queue that will reasonably ensure no one ever spins for practical applications.
   * */
  template <typename CB, typename... ARGS> void enqueueCallbackCommand(CB &&callback, ARGS &&... args) {
    while (this->enqueueCallbackCommandNonblocking(callback, args...) == false) {
      std::this_thread::yield();
    }
  }

  template <typename CB, typename... ARGS>
  bool enqueueReferencingCallbackCommandNonblocking(bool short_circuit, CB &&callback, ARGS &&... args) {
    if (this->headless) {
      callback(args...);
      return true;
    }

    /* If the context isn't running, it's shut down. Currently, contexts can't be restarted, and we don't fully support
     * headless contexts yet. */
    if (this->running.load(std::memory_order_relaxed) == 0) {
      return true;
    }

    return this->command_queue.write(
        [&](auto &cmd) { initReferencingCallbackCommand(&cmd, short_circuit, callback, args...); });
  }

  template <typename CB, typename... ARGS>
  void enqueueReferencingCallbackCommand(bool short_circuit, CB callback, ARGS... args) {
    while (this->enqueueReferencingCallbackCommandNonblocking(short_circuit, callback, args...) == false) {
      std::this_thread::yield();
    }
  }

  template <typename T, typename... ARGS> std::shared_ptr<T> createObject(ARGS &&... args) {
    auto obj = new T(this->getContext(), args...);
    auto ret = sharedPtrDeferred<T>(obj, [](T *ptr) {
      auto ctx = ptr->getContextRaw();
      if (ctx->delete_directly.load(std::memory_order_relaxed) == 0)
        ctx->enqueueDeletionRecord(&deletionCallback<T>, (void *)ptr);
      else
        delete ptr;
    });

    /* Do the second phase of initialization. */
    this->enqueueReferencingCallbackCommand(
        true, [](auto &o) { o->initInAudioThread(); }, obj);

    auto ce = std::static_pointer_cast<CExposable>(ret);
    ce->stashInternalReference(ce);

    return ret;
  }

  /*
   * get the current time since context creation in blocks.
   *
   * This is used for crossfading and other applications.
   * */
  unsigned int getBlockTime() { return this->block_time.load(std::memory_order_relaxed); }

  /*
   * Helpers for the C API. to set properties in the context's thread.
   * These handle the inter-thread synchronization and can be called directly.
   *
   * Eventually this will be extended to handle batched/deferred things as well.
   * */
  void setIntProperty(std::shared_ptr<BaseObject> &obj, int property, int value);
  void setDoubleProperty(std::shared_ptr<BaseObject> &obj, int property, double value);
  void setObjectProperty(std::shared_ptr<BaseObject> &obj, int property, std::shared_ptr<CExposable> &object);
  void setDouble3Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 3> value);
  void setDouble6Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 6> value);
  void setBiquadProperty(std::shared_ptr<BaseObject> &obj, int property, const struct syz_BiquadConfig &value);

  /*
   * Ad a weak reference to the specified source.
   *
   * Handles calling into the audio thread.
   * */
  void registerSource(const std::shared_ptr<Source> &source);

  /*
   * Add a weak reference to the specified global effect.
   *
   * Handles calling into the audio thread.
   * */
  void registerGlobalEffect(const std::shared_ptr<GlobalEffect> &effect);

  /* Helper methods used by various pieces of synthizer to grab global resources. */

  /* Get the direct buffer, which is where things write when they want to bypass panning. This includes effects and
   * direct sources, etc. Inline because it's super inexpensive.
   * */
  float *getDirectBuffer() { return &this->direct_buffer[0]; }

  /* Allocate a panner lane intended to be used by a source. */
  std::shared_ptr<PannerLane> allocateSourcePannerLane(enum SYZ_PANNER_STRATEGY strategy);

  router::Router *getRouter() { return &this->router; }

  /* materialize distance params from the properties. */
  DistanceParams getDefaultDistanceParams() { return materializeDistanceParamsFromDefaultProperties(this); }

  /*
   * Generate a block of audio output for the specified number of channels.
   *
   * The number of channels shouldn't change for the duration of this context in most circumstances.
   * */
  void generateAudio(unsigned int channels, float *output);

  /**
   * Potentially send an event.
   *
   * How this works is you pass it a callback which takes a pointer to an EventBuilder, you build your event, then the
   * context dispatches it on your behalf. The point of this function is that the callback/event building can be
   * entirely avoided if events are disabled; in that cse, your callback will not be called.
   * */
  template <typename CB> void sendEvent(CB &&callback) {
    if (this->event_sender.isEnabled() == false) {
      return;
    }

    EventBuilder builder;
    callback(&builder);
    builder.dispatch(&this->event_sender);
  }

  /**
   * Used by the C API for events:
   * */
  void enableEvents();
  /* May be called from any thread as it is backed by a MPMC queue. */
  void getNextEvent(syz_Event *out);

  /**
   * Begin lingering for an object.
   *
   * Though users can call `syz_configureDeleteBehavior` on any object, it's only really implemented for objects
   * which have access to a context.
   *
   * Handles the `shouldLinger` etc. runaround and won't do anything if the object just wants
   * to die immediately.
   *
   * Should not be called from the audio thread.
   * */
  void doLinger(const std::shared_ptr<BaseObject> &obj);

  /**
   * Enqueue an object to stop lingering immediately. Note that stopLingering is a base class method
   * used as part of the linger machinery, and so the name must be different.  If called
   * as part of the audio tick and assuming the object is already otherwise dead, the object will be dropped
   * before the next audio tick.
   * */
  void enqueueLingerStop(const std::shared_ptr<BaseObject> &obj);

#define PROPERTY_CLASS Context
#define PROPERTY_BASE BaseObject
#define PROPERTY_LIST CONTEXT_PROPERTIES
#include "synthizer/property_impl.hpp"

private:
  bool headless = false;

  /*
   * run all pending commands.
   * */
  void runCommands();

  std::atomic<int> running;
  std::atomic<int> in_audio_callback = 0;
  std::shared_ptr<AudioOutput> audio_output;

  /*
   * Deletion. This queue is read from when the semaphore for the context is incremented.
   *
   * Objects are safe to delete when the iteration of the context at which the deletion was enqueued is greater.
   * This means that all shared_ptr decremented in the previous iteration, and all weak_ptr were invalidated.
   * */
  typedef void (*DeletionCallback)(void *);
  class DeletionRecord {
  public:
    uint64_t iteration;
    DeletionCallback callback;
    void *arg;
  };
  moodycamel::ConcurrentQueue<DeletionRecord> pending_deletes;

  std::atomic<int> delete_directly = 0;
  /*
   * Used to signal that something is queueing a delete. This allows
   * shutdown to synchronize by spin waiting, so that when it goes to drain the deletion queue, it can know that nothing
   * else will appear in it.
   * */
  std::atomic<int> deletes_in_progress = 0;

  void enqueueDeletionRecord(DeletionCallback cb, void *arg);
  /* Used by shutdown and the destructor only. Not safe to call elsewhere. */
  void drainDeletionQueues();

  MpscRing<Command, 10000> command_queue;
  template <typename T> void propertySetter(const std::shared_ptr<BaseObject> &obj, int property, const T &value);

  std::atomic<unsigned int> block_time = 0;

  /* Collections of objects that require execution: sources, etc. all go here eventually. */

  /* direct_buffer is a buffer to which we write when we want to output, bypassing panner banks. */
  std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> direct_buffer;

  /* The key is a raw pointer for easy lookup. */
  deferred_unordered_map<void *, std::weak_ptr<Source>> sources;
  std::shared_ptr<AbstractPannerBank> source_panners = nullptr;

  /* Effects to run. */
  deferred_vector<std::weak_ptr<GlobalEffect>> global_effects;

  /* Parameters of the 3D environment: listener orientation/position, library-wide defaults for distance models, etc. */
  std::array<double, 3> position{{0, 0, 0}};
  /* Default to facing positive y with positive x as east and positive z as up. */
  std::array<double, 6> orientation{{0, 1, 0, 0, 0, 1}};

  /* Effects support. */
  router::Router router{};

  /* Events support. */
  EventSender event_sender;

  /**
   * A queue mapping objects which wish to linger to their linger timeout converted to blocks. Uses std::UINT64_MAX as
   * infinity.
   * */
  PriorityQueue<std::uint64_t, std::weak_ptr<CExposable>> lingering_objects;

  FadeDriver gain_driver{1.0f, 1};
};

} // namespace synthizer