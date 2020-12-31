#pragma once

#include "synthizer.h"

#include "synthizer/trylock.hpp"

#include "plf_colony.h"

#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

#include "config.hpp"

namespace synthizer {

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
 * This is used by all synthizer freeing where possible in order to ensure that memory which must be released on the audio thread
 * is kept to a minimum.
 * */
typedef void freeCallback(void *value);
void deferredFreeCallback(freeCallback *cb, void *value);

/**
 * Drop-in replacement for free that deferrs to a background thread.
 * */
static void deferredFree(void *ptr) {
	deferredFreeCallback(free, ptr);
}

template<typename T>
class DeferredAllocator {
	public:
	typedef T value_type;

	DeferredAllocator() {}

	template<typename U>
	DeferredAllocator(const DeferredAllocator<U> &other) {}
	template<typename U>
	DeferredAllocator(U &&other) {}

	value_type *allocate(std::size_t n) {
		void *ret;
		ret = std::malloc(sizeof(value_type) * n);
		if (ret == nullptr) {
			throw std::bad_alloc();
		}
		return (value_type *)ret;
	}

	void deallocate(T *p, std::size_t n) {
		deferredFree((void *)p);
	}
};

template<typename T>
bool operator==(const DeferredAllocator<T> &a, const DeferredAllocator<T> &b) {
	return true;
}

/*
 * Typedefs for std and plf types that are deferred.
 * This saves us from std::vector<std::shared_ptr<...> DeferredAllocator<std::shared_ptr<...>>> fun.
 * weird casing is to match std and colony.
 * */
template<typename T>
using deferred_vector = std::vector<T, DeferredAllocator<T>>;

template<typename K, typename V, typename HASH = std::hash<K>, typename KE = std::equal_to<K>>
using deferred_unordered_map = std::unordered_map<K, V, HASH, KE, DeferredAllocator<std::pair<const K, V>>>;

template<typename T, typename SKIPFIELD_T = unsigned short>
using deferred_colony = plf::colony<T, DeferredAllocator<T>, SKIPFIELD_T>;

/*
 * makes shared_ptrs with the shared_ptr constructor, but injects a DeferredAllocator.
 * */
template<typename T, typename... ARGS>
std::shared_ptr<T> sharedPtrDeferred(ARGS&&... args) {
	return std::shared_ptr<T>(std::forward<ARGS>(args)..., DeferredAllocator<T>());
}

/*
 * Like std::allocate_shared but doesn't make us specify the allocator types.
 * */
template<typename T, typename... ARGS>
std::shared_ptr<T> allocateSharedDeferred(ARGS&&... args) {
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

class CExposable  {
	public:
	CExposable();
	virtual ~CExposable() {}

	/* May be zero. You shouldn't use this directly. */
	syz_Handle getCHandle() {
		return this->c_handle.load(std::memory_order_relaxed);
	}

	/* Returns false if the handle is already set. */
	bool  setCHandle(syz_Handle handle) {
		syz_Handle zero = 0;
		return this->c_handle.compare_exchange_strong(zero, handle, std::memory_order_relaxed);
	}

	/**
	 * Should return one of the SYZ_OTYPE_ constants.
	 * */
	virtual int getObjectType() = 0;

	void *getUserdata();
	void setUserdata(void *userdata, syz_UserdataFreeCallback *userdata_free_callback);

	bool isPermanentlyDead() {
		return this->permanently_dead.load(std::memory_order_relaxed) == 1;
	}

	/**
	 * Called from the C API to hide this object from the C API.  Since objects can linger until the next context tick,
	 * we want to hide the delete latency by immediately considering the object invalid.
	 * 
	 * Returns true if the object was alive previously. Used to ensure only one caller to cDelete.
	 * */
	bool becomePermanentlyDead() {
		unsigned char zero = 0;
		return this->permanently_dead.compare_exchange_strong(zero, 1, std::memory_order_relaxed);
	}

	/*
	 * Hook to know that the C side deleted something.
	 * 
	 * This exists primarily so that the context can shut down in the foreground; it is otherwise possible for it to keep running until after main.
	 * */
	virtual void cDelete() {}

	private:
	std::atomic<syz_Handle> c_handle;
	std::atomic<unsigned char> permanently_dead = 0;
	TryLock<UserdataDef> userdata{};
};

void freeCImpl(std::shared_ptr<CExposable> &obj);

template<typename T>
void freeC(std::shared_ptr<T> &obj) {
	std::shared_ptr<CExposable> b = std::static_pointer_cast<CExposable>(obj);
	freeCImpl(obj);
}

syz_Handle getCHandleImpl(std::shared_ptr<CExposable> &&obj);

/*
 * Safely convert an object into a C handle which can be passed to the external world.
 * 
 * Returns 0 if the object can't be exposed, since this is 99% used to pass handles out.
 * */
template<typename T>
syz_Handle toC(T&& obj) {
	if (obj) {
		return getCHandleImpl(std::move(std::static_pointer_cast<CExposable>(obj)));
	} else {
		return 0;
	}
}

/* Throws EInvalidHandle. */
std::shared_ptr<CExposable> getExposableFromHandle(syz_Handle handle);

/* Throws EType if the handle is of the wrong type. */
template<typename T>
std::shared_ptr<T> fromC(syz_Handle handle) {
	auto h = getExposableFromHandle(handle);
	auto ret = std::dynamic_pointer_cast<T>(h);
	if (ret == nullptr) throw EHandleType();
	return ret;
}

/**
 * Does a dynamic_pointer_cast, throwing EHandleType instead of returning nullptr.
 * 
 * This is used for things like Pausable, where the inheritance hierarchy forces us to use mixins but we still need to sidestep to BaseObject.
 * */
template<typename O, typename I>
std::shared_ptr<O> typeCheckedDynamicCast(const std::shared_ptr<I> &input) {
	auto out = std::dynamic_pointer_cast<O>(input);
	if (out == nullptr) {
		throw EHandleType();
	}
	return out;
}

void clearAllCHandles();

}