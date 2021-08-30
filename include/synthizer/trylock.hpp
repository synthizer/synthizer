#pragma once

#include "synthizer/at_scope_exit.hpp"

#include <atomic>
#include <utility>

namespace synthizer {

/**
 * A TryLock is a lock that only supports try_lock.  The reason to use this
 * over a standard std::mutex is that it's backed by an atomic and suitable for use in the audio thread
 * in cases where the algorithm in question is able to deal with not locking (e.G. various kinds of cache,
 * acquiring values with nontrivial constructors, etc).
 *
 * This also protects data in a Rust-ish fashion: you provide a pair of callbacks, and the callbacks get called instead
 * of being able to try_lock directly.
 * */
template <typename INNER_T> class TryLock {
public:
  /**
   * Construct a TryLock using INNER_T's default constructor.
   * */
  TryLock() : data() {}

  /**
   * Construct a TryLock using any constructor of INNER_T, forwarding args.
   * */
  template <typename... ARGS> TryLock(ARGS &&... args) : data(std::forward(args)...) {}

  /**
   * Call the first callback if and only if it was possible to lock the lock.
   * Call the second callback if locking the lock wasn't possible.  The first callback
   * receives a pointer to the protected data.
   *
   * The lock unlocks when the callback returns.
   * Returns true if the lock was successfully locked.
   * */
  template <typename CB1, typename CB2> bool withLock(CB1 &&callback_locked, CB2 &&callback_failed) {
    bool is_locked = this->try_lock();
    if (is_locked) {
      auto unlocker = AtScopeExit([&]() { this->unlock(); });
      callback_locked(&this->data);
      return true;
    } else {
      callback_failed();
      return false;
    }
  }

  /**
   * Call the callback if the lock was locked successfully, then return whether the lock was locked.
   * Shorthand for the two-parameter withLock where the second callback does nothing.
   * */
  template <typename CB> bool withLock(CB &&callback) {
    return this->withLock(callback, []() {});
  }

  INNER_T *unsafeGetInner() { return &this->data; }

private:
  bool try_lock() {
    int expected = 0;
    if (this->locked.load(std::memory_order_relaxed) == 1 ||
        this->locked.compare_exchange_strong(expected, 0, std::memory_order_acquire, std::memory_order_relaxed) ==
            false) {
      return false;
    }
    return true;
  }

  void unlock() { this->locked.store(0, std::memory_order_release); }

  INNER_T data;
  std::atomic<int> locked = 0;
};

} // namespace synthizer