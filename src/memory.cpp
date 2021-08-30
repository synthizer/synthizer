#include "synthizer.h"

#include "synthizer/error.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/vector_helpers.hpp"

#include <concurrentqueue.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <utility>

namespace synthizer {

CExposable::CExposable() {}

std::shared_ptr<CExposable> getExposableFromHandle(syz_Handle handle) {
  CExposable *exposable = (CExposable *)handle;
  auto out = exposable->getInternalReference();
  if (out == nullptr || out->isPermanentlyDead()) {
    throw EInvalidHandle("This handle is already dead. Synthizer cannot catch all cases of invalid handles; change "
                         "your program to use only valid handles or risk crashes.");
  }
  return out;
}

/*
 * Infrastructure for deferred frees and holding handles
 * to delete on library shutdown.
 * */

static std::mutex registered_handles_lock{};
static deferred_vector<std::weak_ptr<CExposable>> registered_handles;

/* Get rid of any handles which have died. */
static void purgeDeadRegisteredHandles() {
  auto guard = std::lock_guard(registered_handles_lock);

  vector_helpers::filter(registered_handles, [](auto &p) { return p.lock() != nullptr; });
}

void registerObjectForShutdownImpl(const std::shared_ptr<CExposable> &obj) {
  purgeDeadRegisteredHandles();

  auto guard = std::lock_guard(registered_handles_lock);
  registered_handles.emplace_back(obj);
}

struct DeferredFreeEntry {
  freeCallback *cb;
  void *value;
};

/*
 * The queue. 1000 items and 64 of each type of producer.
 * */
static moodycamel::ConcurrentQueue<DeferredFreeEntry> deferred_free_queue{1000, 64, 64};
static std::thread deferred_free_thread;
static std::atomic<int> deferred_free_thread_running = 0;
thread_local static bool is_deferred_free_thread = false;

/*
 * Drain the deferred freeing callback queue and drain all registered handles which are now dead.
 * */
unsigned int deferredFreeTick() {
  decltype(deferred_free_queue)::consumer_token_t token{deferred_free_queue};
  DeferredFreeEntry ent;
  unsigned int processed = 0;

  while (deferred_free_queue.try_dequeue(token, ent)) {
    try {
      ent.cb(ent.value);
    } catch (...) {
      logDebug("Exception on memory freeing thread. This should never happen");
    }
    processed++;
  }

  purgeDeadRegisteredHandles();
  return processed;
}

static void deferredFreeWorker() {
  std::size_t processed = 0;
  is_deferred_free_thread = true;

  while (deferred_free_thread_running.load(std::memory_order_relaxed)) {
    processed += deferredFreeTick();
    /* Sleep for a bit so that we don't overload the system when we're not freeing. */
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  logDebug("Deferred free processed %zu frees in a background thread", processed);
}

void deferredFreeCallback(freeCallback *cb, void *value) {
  if (deferred_free_thread_running.load() == 0) {
    cb(value);
    return;
  }

  if (value == nullptr) {
    return;
  }

  if (is_deferred_free_thread || deferred_free_queue.try_enqueue({cb, value}) == false) {
    cb(value);
  }
}

void clearAllCHandles() {
  auto guard = std::lock_guard(registered_handles_lock);

  for (auto &h : registered_handles) {
    auto s = h.lock();
    if (s == nullptr) {
      continue;
    }
    s->dieNow();
  }

  return;
}

void initializeMemorySubsystem() {
  deferred_free_thread_running.store(1);
  deferred_free_thread = std::thread{deferredFreeWorker};
}

void shutdownMemorySubsystem() {
  deferred_free_thread_running.store(0);
  deferred_free_thread.join();
  /*
   * Now get rid of anything that might still be around.
   */
  deferredFreeTick();
}

UserdataDef::~UserdataDef() { this->maybeFreeUserdata(); }

void UserdataDef::set(void *ud, syz_UserdataFreeCallback *ud_free_callback) {
  this->maybeFreeUserdata();
  this->userdata.store(ud, std::memory_order_relaxed);
  this->userdata_free_callback = ud_free_callback;
}

void *UserdataDef::getAtomic() { return this->userdata.load(std::memory_order_relaxed); }

void UserdataDef::maybeFreeUserdata() {
  void *ud = this->userdata.load(std::memory_order_relaxed);
  if (ud != nullptr && this->userdata_free_callback != nullptr) {
    deferredFreeCallback(this->userdata_free_callback, ud);
  }
  this->userdata = nullptr;
  this->userdata_free_callback = nullptr;
}

void *CExposable::getUserdata() {
  auto *inner = this->userdata.unsafeGetInner();
  return inner->getAtomic();
}

void CExposable::setUserdata(void *ud, syz_UserdataFreeCallback *ud_free_callback) {
  bool did_set = this->userdata.withLock([&](auto *u) { u->set(ud, ud_free_callback); });

  /* We lost the race. */
  if (did_set == false && ud != nullptr && ud_free_callback != nullptr) {
    deferredFreeCallback(ud_free_callback, ud);
  }
}

} // namespace synthizer
