#pragma once

#include "synthizer.h"

#include "synthizer/error.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <new>

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

	syz_Handle getCHandle() {
		return this->c_handle;
	}

	/*
	 * Hook to know that the C side deleted something.
	 * 
	 * This exists primarily so that the context can shut down in the foreground; it is otherwise possible for it to keep running until after main.
	 * */
	virtual void cDelete() {}

	private:
	syz_Handle c_handle;
};

/*
 * Given an object, increment the C-side reference count.
 * */
void incRefCImpl(std::shared_ptr<CExposable> &obj);

template<typename T>
void incRefC(std::shared_ptr<T> &obj) {
	std::shared_ptr<CExposable> b = std::static_pointer_cast<CExposable>(obj);
	return incRefCImpl(b);
}

void decRefCImpl(std::shared_ptr<CExposable> &obj);

template<typename T>
void decRefC(std::shared_ptr<T> &obj) {
	std::shared_ptr<CExposable> b = std::static_pointer_cast<CExposable>(obj);
	decRefCImpl(obj);
}

/*
 * Safely convert an object into a C handle which can be passed to the external world.
 * 
 * Expectes std::shared_ptr, Does the first refcount. Unfortunately we need this to be a forwarding reference so the type can't constrain it further.
 * */
template<typename T>
syz_Handle toC(T &&obj) {
	incRefC(obj);
	return obj->getCHandle();
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