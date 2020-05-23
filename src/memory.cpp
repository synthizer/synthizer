#include "synthizer.h"
#include "synthizer/memory.hpp"

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace synthizer {

syz_Handle allocateCHandle() {
	static std::atomic<syz_Handle> highest_handle = 1;
	return highest_handle.fetch_add(1, std::memory_order_relaxed);
}

CExposable::CExposable() {
	this->c_handle = allocateCHandle();
}

struct HandleEntry {
	std::atomic<unsigned int> refcount;
	std::shared_ptr<CExposable> value;
};

static std::shared_mutex c_handles_mutex;
static std::unordered_map<syz_Handle, HandleEntry> c_handles;

void incRefCImpl(std::shared_ptr<CExposable> &obj) {
	{
		std::shared_lock l(c_handles_mutex);
		auto elem = c_handles.find(obj->getCHandle());
		if (elem != c_handles.end()) {
			elem->second.refcount.fetch_add(1, std::memory_order_relaxed);
			return;
		}
	}

	/* Otherwise we need the exclusive lock, and will create the entry. */
	{
		std::unique_lock l(c_handles_mutex);
		auto &rec = c_handles[obj->getCHandle()];
		rec.value = obj;
		rec.refcount.store(1, std::memory_order_relaxed);
	}
}

void decRefCImpl(std::shared_ptr<CExposable> &obj) {
	bool should_delete = false;

	{
		std::shared_lock l(c_handles_mutex);
		auto elem = c_handles.find(obj->getCHandle());
		assert(elem != c_handles.end());
		auto old = elem->second.refcount.fetch_sub(1, std::memory_order_relaxed);
		assert(old != 0); // If 0, we tried to decref more than incref.
		should_delete = (old <= 1);
	}

	/*
	 * We do deletion in a second pass; this requires the unique lock.
	 * The entry stays in the map until one of these is hit, this actually drops the reference.
	 * */
	if (should_delete) {
		std::unique_lock l(c_handles_mutex);
		c_handles.erase(obj->getCHandle());
	}
}

std::shared_ptr<CExposable> getExposableFromHandle(syz_Handle handle) {
	std::shared_lock l(c_handles_mutex);
	auto it = c_handles.find(handle);
	if (it == c_handles.end()) throw EInvalidHandle();
	return it->second.value;
}

void clearAllCHandles() {
	auto g = std::lock_guard(c_handles_mutex);
	c_handles.clear();
}

}