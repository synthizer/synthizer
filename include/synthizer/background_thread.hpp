#pragma once

#include "synthizer/logging.hpp"

#include "sema.h"
#include <concurrentqueue.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <thread>

namespace synthizer {

/*
 * A background thread for doing things that might block, for example some forms of memory freeing or callbacks.
 *
 * This serializes everything it into one thread and shouldn't be used for intensive work. The point is to be able to
 * defer stuff that talks to the kernel or the user's code so that it's not running on threads that do actually do the
 * work; in future this abstraction might be replaced with something better, but for now it fulfills our needs.
 *
 * This can allocate in callInBackground, but asymptotically doesn't depending on load, at the cost of permanent ram
 * usage.
 * TODO: do we need a high watermark?
 *
 * There is one exception to the deferred behavior: during library shutdown, allocation happens in the foreground
 * to avoid issues with the static queue dying.
 * */

/* Start/stop should only be called as part of library initialization/deinitialization for now. These functions are not
 * threadsafe, nor can the thread be safely started more than once. */
void startBackgroundThread();
void stopBackgroundThread();

typedef void (*BackgroundThreadCallback)(void *);
void callInBackground(BackgroundThreadCallback cb, void *arg);

template <typename T> void _deferredBackgroundDelete(void *ptr) { delete (T *)ptr; }

/*
 * A deleter for a shared_ptr, which will defer the delete to the background thread.
 * */
template <typename T> void deleteInBackground(T *ptr) { callInBackground(_deferredBackgroundDelete<T>, (void *)ptr); }

namespace background_thread_detail {
class BackgroundThreadCommand {
public:
  BackgroundThreadCallback callback;
  void *arg;
};

inline moodycamel::ConcurrentQueue<BackgroundThreadCommand> background_queue;
inline Semaphore background_thread_semaphore;
inline std::atomic<int> background_thread_running = 0;

inline void backgroundThreadFunc() {
  BackgroundThreadCommand cmd;

  setThreadPurpose("background-thread");
  logDebug("Background thread started");
  while (background_thread_running.load(std::memory_order_relaxed)) {
    while (background_queue.try_dequeue(cmd)) {
      try {
        cmd.callback(cmd.arg);
      } catch (...) {
        logError("Exception on background thread. This should never happen");
      }
    }
    background_thread_semaphore.wait();
  }

  /*
   * At exit, drain the queue.
   * This prevents things like the context from having threads running past the end of main.
   * */
  while (background_queue.try_dequeue(cmd)) {
    cmd.callback(cmd.arg);
  }

  logDebug("Background thread stopped");
}

inline std::thread background_thread;
} // namespace background_thread_detail

inline void startBackgroundThread() {
  using namespace background_thread_detail;

  background_thread_running.store(1, std::memory_order_relaxed);
  background_thread = std::thread(backgroundThreadFunc);
}

inline void stopBackgroundThread() {
  using namespace background_thread_detail;

  background_thread_running.store(0, std::memory_order_relaxed);
  background_thread_semaphore.signal();
  background_thread.join();
}

inline void callInBackground(BackgroundThreadCallback cb, void *arg) {
  using namespace background_thread_detail;

  if (background_thread_running.load() == 0) {
    cb(arg);
    return;
  }

  BackgroundThreadCommand cmd;
  cmd.callback = cb;
  cmd.arg = arg;
  while (background_queue.enqueue(cmd) == false)
    ;
}

} // namespace synthizer