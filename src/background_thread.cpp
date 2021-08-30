#include "synthizer/background_thread.hpp"

#include "synthizer/logging.hpp"

#include "sema.h"
#include <concurrentqueue.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <thread>

namespace synthizer {

class BackgroundThreadCommand {
public:
  BackgroundThreadCallback callback;
  void *arg;
};

static moodycamel::ConcurrentQueue<BackgroundThreadCommand> background_queue;
static Semaphore background_thread_semaphore;
static std::atomic<int> background_thread_running = 0;

static void backgroundThreadFunc() {
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

static std::thread background_thread;
void startBackgroundThread() {
  background_thread_running.store(1, std::memory_order_relaxed);
  background_thread = std::thread(backgroundThreadFunc);
}

void stopBackgroundThread() {
  background_thread_running.store(0, std::memory_order_relaxed);
  background_thread_semaphore.signal();
  background_thread.join();
}

void callInBackground(BackgroundThreadCallback cb, void *arg) {
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