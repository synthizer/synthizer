#pragma once

#include "synthizer.h"

#include "synthizer/cells.hpp"
#include "synthizer/error.hpp"
#include "synthizer/trylock.hpp"

#include "plf_colony.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

#include "config.hpp"

namespace synthizer {

class CExposable;

void initializeMemorySubsystem();
void shutdownMemorySubsystem();

/*
 * Defer a free to a background thread which wakes up periodically.
 * This function doesn't allocate, but under high memory pressure it can opt to free in the calling thread.
 * If it doesn't free on the calling thread, then it doesn't enter the kernel.
 *
 * The callback is so that we can choose whether to go through any of a variety of mechanisms,
 * at the moment freeAligned and delete[].
 *
 * This is used by all synthizer freeing where possible in order to ensure that memory which must be released on the
 * audio thread is kept to a minimum.
 * */
typedef void freeCallback(void *value);
void deferredFreeCallback(freeCallback *cb, void *value);

/**
 * Drop-in replacement for free that deferrs to a background thread.
 * */
inline void deferredFree(void *ptr) { deferredFreeCallback(free, ptr); }

template <typename T> void _deferredDeleteImpl(void *obj) { delete static_cast<T *>(obj); }

/*
 * Drop-in replacement for delete obj, but deferred to a background thread.
 * */
template <typename T> void deferredDelete(T *obj) { deferredFreeCallback(_deferredDeleteImpl<T>, (void *)obj); }

template <typename T> class DeferredAllocator {
public:
  typedef T value_type;

  DeferredAllocator() {}

  template <typename U> DeferredAllocator(const DeferredAllocator<U> &other) { (void)other; }

  template <typename U> DeferredAllocator(U &&other) { (void)other; }

  value_type *allocate(std::size_t n) {
    void *ret;
    ret = std::malloc(sizeof(value_type) * n);
    if (ret == nullptr) {
      throw std::bad_alloc();
    }
    return (value_type *)ret;
  }

  void deallocate(T *p, std::size_t n) {
    (void)n;
    deferredFree((void *)p);
  }

  bool operator==(const DeferredAllocator &other) { return true; }

  bool operator!=(const DeferredAllocator &other) { return false; }
};

template <typename T> bool operator==(const DeferredAllocator<T> &a, const DeferredAllocator<T> &b) { return true; }

/*
 * Typedefs for std and plf types that are deferred.
 * This saves us from std::vector<std::shared_ptr<...> DeferredAllocator<std::shared_ptr<...>>> fun.
 * weird casing is to match std and colony.
 * */
template <typename T> using deferred_vector = std::vector<T, DeferredAllocator<T>>;
template <class Key, class T, class Compare = std::less<Key>>
using deferred_map = std::map<Key, T, Compare, DeferredAllocator<std::pair<const Key, T>>>;
template <class Key, class Compare = std::less<Key>>
using deferred_set = std::set<Key, Compare, DeferredAllocator<Key>>;
template <typename K, typename V, typename HASH = std::hash<K>, typename KE = std::equal_to<K>>
using deferred_unordered_map = std::unordered_map<K, V, HASH, KE, DeferredAllocator<std::pair<const K, V>>>;

template <typename T, typename SKIPFIELD_T = unsigned short>
using deferred_colony = plf::colony<T, DeferredAllocator<T>, SKIPFIELD_T>;

/*
 * makes shared_ptrs with the shared_ptr constructor, but injects a DeferredAllocator.
 * */
template <typename T, typename... ARGS> std::shared_ptr<T> sharedPtrDeferred(ARGS &&... args) {
  return std::shared_ptr<T>(std::forward<ARGS>(args)..., DeferredAllocator<T>());
}

/*
 * Like std::allocate_shared but doesn't make us specify the allocator types.
 * */
template <typename T, typename... ARGS> std::shared_ptr<T> allocateSharedDeferred(ARGS &&... args) {
  return std::allocate_shared<T, DeferredAllocator<T>, ARGS...>(DeferredAllocator<T>(), std::forward<ARGS>(args)...);
}

/*
 * Infrastructure for marshalling C objects to/from Synthizer.
 * */

class UserdataDef {
public:
  ~UserdataDef();
  void *getAtomic();
  void set(void *userdata, syz_UserdataFreeCallback *userdata_free_callback);

private:
  void maybeFreeUserdata();
  std::atomic<void *> userdata = nullptr;
  syz_UserdataFreeCallback *userdata_free_callback = nullptr;
};

/*
 * Register an object for deletion at Synthizer shutdown.
 *
 * NOTE: uses a lock; shouldn't be called from the audio thread.
 * */
void registerObjectForShutdownImpl(const std::shared_ptr<CExposable> &obj);
template <typename T> void registerObjectForShutdown(const std::shared_ptr<T> &obj) {
  auto ce = std::static_pointer_cast<CExposable>(obj);
  registerObjectForShutdownImpl(ce);
}

void clearAllCHandles();

/**
 * A reference-counted external reference to C.
 *
 * Supports:
 * - Stashing userdata, with a free callback.
 * - Reference counting operations.
 * */
class CExposable : public std::enable_shared_from_this<CExposable> {
public:
  CExposable();
  virtual ~CExposable() {}

  syz_Handle getCHandle() { return (syz_Handle)this; }

  /**
   * Should return one of the SYZ_OTYPE_ constants.
   * */
  virtual int getObjectType() = 0;

  void *getUserdata();
  void setUserdata(void *userdata, syz_UserdataFreeCallback *userdata_free_callback);

  /*
   * Though objects are reference counted, it is possible for them to stay alive slightly past their destruction if the
   * audio threads or the object itself is holding a reference; to detect this case and prevent users from accessing
   * them, this function detects whether the object should be considered alive.
   * */
  bool isPermanentlyDead() { return this->reference_count.load(std::memory_order_relaxed) == 0; }

  /*
   * Hook to know that the C side deleted something.
   *
   * This exists primarily so that the context can shut down in the foreground; it is otherwise possible for it to keep
   * running until after main.
   * */
  virtual void cDelete() {}

  /**
   * These 3 functions are used to expose an object to the external world.
   * They work as follows:
   *
   * - stashInternalReference stashes a shared_ptr to this object in this object and sets the reference count to 1.  It
   * also registeres that reference in the global registry as an object which should be deleted. This is done here
   * because there are too many custom ways to make objects for us to abhstract that generally, but they all must pass
   * through this function.
   * - incRef increments the reference count of this object if it is nonzero, and ignores increments if it is zero.
   * - decRef decrements the reference count of this object to (but not below) 0.
   * - users of the object detect that the reference count is zero using isPermanentlyDead; once this happens,
   *   no increments are allowed to happen.
   *
   * The object should never have these functions accessed save through a shared_ptr to the object, since doing so can
   * decrement the reference count.  What actually keeps the object alive from the internal perspective is whether a
   * shared_ptr exists, and these functions just manage an internal reference.  If no shared_ptr exists and the object
   * reaches a reference count of 0, it immediately dies.
   * */
  void stashInternalReference(const std::shared_ptr<CExposable> &reference) {
    this->internal_reference = reference;
    this->reference_count.store(1, std::memory_order_relaxed);
    registerObjectForShutdown(reference);
  }

  /* Returns whether or not the reference count was already 0. Used primarily by the event architecture. */
  bool incRef() {
    unsigned int cur = this->reference_count.load(std::memory_order_relaxed);
    while (cur != 0) {
      if (this->reference_count.compare_exchange_strong(cur, cur + 1, std::memory_order_acquire,
                                                        std::memory_order_relaxed)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the new reference count, which is used to know if this object will be dropped at the end of `syz_decRef`.
   * */
  unsigned int decRef() {
    unsigned int cur = this->reference_count.load(std::memory_order_relaxed);
    assert(cur != 0 && "Too many reference count decrements");
    while (cur != 0) {
      if (this->reference_count.compare_exchange_strong(cur, cur - 1, std::memory_order_release,
                                                        std::memory_order_relaxed)) {
        break;
      }
    }
    /* Be careful: compare_exchange_strong doesn't set cur when it succeeds. */
    if (cur == 1) {
      this->internal_reference = nullptr;
    }
    return cur - 1;
  }

  std::shared_ptr<CExposable> getInternalReference() { return this->internal_reference; }

  /*
   * At library shutdown, it is necessary to be able to kill all objects. This function accomplishes that and should not
   * be used for any other purpose.
   * */
  void dieNow() {
    this->internal_reference = nullptr;
    this->linger_reference = nullptr;
  }

  struct syz_DeleteBehaviorConfig getDeleteBehaviorConfig() {
    return this->delete_behavior.read();
  }

  void setDeleteBehaviorConfig(struct syz_DeleteBehaviorConfig cfg) { this->delete_behavior.write(cfg); }

  /**
   * Return true if this object wants to linger.
   * Buffers, contexts, etc. return false. Generators, etc. with
   * implemented linger behavior return true
   *
   * used to optimize object deletion by not enqueueing things in the priority queue.
   *
   * May be called from any thread.
   * */
  virtual bool wantsLinger() { return false; }

  /**
   * Stash an internal reference to keep this object alive. Also tell derived classes that lingering has begun.
   *
   * Should return a suggested timeout in seconds for lingering. This is e.g. the length of the buffer in a
   * BufferGenerator.
   *
   * `configured_timeout` is the currently configured linger timeout, which is retrieved by the caller so that the
   * `LatchCell` doesn't need to do a rather complicated runaround for us more than once.
   *
   * Will be called in the audio thread.
   *
   * If this function returns an empty optional, the object will handle ending the linger itself.
   *
   * The derived class must always call this function so that internal references may be properly assigned, but may
   * otherwise ignore the return value.
   * */
  virtual std::optional<double> startLingering(const std::shared_ptr<CExposable> &reference,
                                               double configured_timeout) {
    this->linger_reference = reference;
    return std::optional(configured_timeout);
  }

  /**
   * Signal a linger stop point, a point at which
   * an object should die if it has opted to linger.  For objects associated with an audio thread (aka anything
   * inheriting from BaseObject) this function should only ever be called from the audio thread.
   *
   * If called while the object isn't lingering, does nothing.
   * */
  virtual void signalLingerStopPoint() {}

private:
  friend class BaseObject;

  /*
   * Reference counts start at 0 because if this object is internal to the library,then the object won't be
   * accessible to the user and is kept alive by shared_ptr and weak_ptr.
   *
   * See the comments on incRef and decRef for how this works.
   * */
  std::atomic<unsigned int> reference_count = 0;
  /*
   * keeps this object alive until set to nullptr.
   * */
  std::shared_ptr<CExposable> internal_reference = nullptr;
  /**
   * Keeps this object alive when lingering.
   * */
  std::shared_ptr<CExposable> linger_reference = nullptr;

  std::atomic<unsigned char> permanently_dead = 0;
  TryLock<UserdataDef> userdata{};
  LatchCell<struct syz_DeleteBehaviorConfig> delete_behavior;
};

/*
 * Convert an object into a C handle which can be passed to the external world.
 *
 * Returns 0 if the object is nullptr.
 * */
template <typename T> syz_Handle toC(const std::shared_ptr<T> &obj) {
  if (obj == nullptr) {
    return 0;
  }
  return obj->getCHandle();
}

/* Throws EInvalidHandle. */
std::shared_ptr<CExposable> getExposableFromHandle(syz_Handle handle);

/* Throws EType if the handle is of the wrong type. */
template <typename T> std::shared_ptr<T> fromC(syz_Handle handle) {
  auto h = getExposableFromHandle(handle);
  auto ret = std::dynamic_pointer_cast<T>(h);
  if (ret == nullptr)
    throw EHandleType();
  return ret;
}

/**
 * Does a dynamic_pointer_cast, throwing EHandleType instead of returning nullptr.
 *
 * This is used for things like Pausable, where the inheritance hierarchy forces us to use mixins but we still need to
 * sidestep to BaseObject.
 * */
template <typename O, typename I> std::shared_ptr<O> typeCheckedDynamicCast(const std::shared_ptr<I> &input) {
  auto out = std::dynamic_pointer_cast<O>(input);
  if (out == nullptr) {
    throw EHandleType();
  }
  return out;
}

} // namespace synthizer
