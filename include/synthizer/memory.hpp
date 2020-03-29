#pragma once

#include <algorithm>
#include <cstdlib>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#endif

#include "config.hpp"

namespace synthizer {

/*
 * The main point of this file is to provide an aligned allocator/free mechanism.
 * 
 * Unfortunately windows doesn't have aligned_alloc/free, so we have to route through here for portability.
 * */
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
}