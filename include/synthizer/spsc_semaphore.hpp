#pragma once

/*
 * cpp11-on-multicore is broken and we need to bring errno into scope for it.
 * */
// clang-format off
// The header order is important here, so that we don't have to fork sema.h.
#include <errno.h>
#include "sema.h"
// clang-format on

#include <atomic>
#include <vector>

namespace synthizer {

/*
 * This is a single-producer single-consumer semaphore optimized for low concurrency and frequent
 * construction/destruction.
 *
 * This works by using an atomic counter and a shared set of semaphores, and is intentionally mostly inline.
 *
 * The producer and consumer must always be the same threads: you can't protect this with a mutex and expect it to work.
 *
 * This works by backing these semaphores onto a pool of thread-local semaphores.
 * */

class SPSCSemaphore {
public:
  SPSCSemaphore() {
    if (SPSCSemaphore::sema_pool.size() > 0) {
      this->backing_sema = SPSCSemaphore::sema_pool.back();
      SPSCSemaphore::sema_pool.pop_back();
    } else {
      this->backing_sema = new Semaphore();
    }
  }

  ~SPSCSemaphore() { SPSCSemaphore::sema_pool.push_back(this->backing_sema); }

  void signal() { this->backing_sema->signal(); }

  void wait() { this->backing_sema->wait(); }

private:
  Semaphore *backing_sema;
  static thread_local std::vector<Semaphore *> sema_pool;
};

} // namespace synthizer