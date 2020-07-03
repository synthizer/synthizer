#pragma once

#include "synthizer.h"

#include "synthizer/error.hpp"

#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

#ifdef _WIN32
#include <malloc.h>
#endif

#include "config.hpp"

namespace synthizer {

#ifdef _WIN32
template<typename T>
T* allocAligned(std::size_t elements, std::size_t alignment = config::ALIGNMENT) {
	void *d = _aligned_malloc(elements * sizeof(T), std::max(alignment, alignof(T)));
	if (d == nullptr)
		throw std::bad_alloc();

	return (T*) d;
}

template<typename T>
void freeAligned(T* ptr) {
	_aligned_free((void*)ptr);
}

#else

template<typename T>
T* allocAligned(std::size_t elements, std::size_t alignment = config::ALIGNMENT) {
	void *d = std::aligned_alloc(std::max(alignment, alignof(T)), elements * sizeof(T));
	if (d == nullptr)
		throw std::bad_alloc();
	return (T*) d;
}

template<typename T>
void freeAligned(T* ptr) {
	std::free((void*)ptr);
}
#endif

/*
 * Infrastructure for marshalling C objects to/from Synthizer.
 * */

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

	bool isPermanentlyDead() {
		return this->permanently_dead.load(std::memory_order_relaxed) == 1;
	}

	/* Returns true if the object was alive previously. Used to ensure only one caller to cDelete. */
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
	return getCHandleImpl(std::move(std::static_pointer_cast<CExposable>(obj)));
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

void clearAllCHandles();

}