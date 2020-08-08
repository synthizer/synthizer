#include "synthizer.h"

#include "synthizer/concurrent_slab.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"

#include "concurrentqueue.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <utility>

namespace synthizer {

CExposable::CExposable() {
	this->c_handle.store(0);
}

struct HandleCell {
	std::shared_ptr<CExposable> value = nullptr;
	unsigned int multiple = 0;
};

static void handleCellDeinit(std::size_t index, HandleCell &cell) {
	try {
		cell.value = nullptr;
	} catch(...) {
		/*
		 * Swallow the error. Assumes that Synthizer never throws.
		 * NOTE: once we have object type codes or something, format this log message better so we know which destructor.
		 * */
		logError("C-facing object destructor threw and the exception was swalloed.");
	}
}

static concurrent_slab::ConcurrentSlab<HandleCell, decltype(handleCellDeinit) *, 65535> handle_slab(handleCellDeinit);

/*
 * Here's how this works.
 * 
 * Objects start off with no handle, indicating that they've never been exposed to the C API.
 * 
 * On the first egress, we allocate a handle to the object if we can, otherwise we fail with an error. Since the slab is 65535 elements, you'd have to have 65535 objects concurrently to outdo it and, if this is a problem, we'll bump the slab to 100000 or so.
 * 
 * The handle is the index of the allocated slab, or some multiple thereof, so we get it as cell.multiple * capacity.
 * Multiples start at 1, though this isn't immediately evident from the code, so this expression is never zero.
 * 
 * On free, the object immediately gets a flag that indicates that we're not allowed to expose it to the C API anymore, then the cell gets deallocated. We can detect that an object shouldn't be exposed by checking this flag,
 * or because the cell is deallocated.
 * 
 * The user-facing impact of this is that objects which are deleted by the user but still held onto internally are not exposable again. Most of the time we use std::weak_ptr,
 * so the object just gets deleted; cases where something might keep an object alive past what C can see will be documented in the manual, as well as a more user-friendly version of this comment. It would be nice to be able to
 * just do what WebAudio does where objects exist as long as they're needed, but to get that you kind of need GC integration and have to exclude
 * anything at all being able to read objects, including debugging code without special knowledge of the runtime (i.e.e in WebAudio, you debug it via Chrome, which looks into the world from outside).
 * */

syz_Handle getCHandleImpl(std::shared_ptr<CExposable> &&obj) {
	auto proposed = obj->getCHandle();
	if (proposed == 0) {
		/* Have to allocate a cell. */
		try {
			CExposable *ptr = obj.get();
			std::size_t index;
			syz_Handle handle;
			handle_slab.allocateCell([&] (std::size_t i, HandleCell &cell) {
				index = i;
				cell.multiple++;
				cell.value = std::move(obj);
				handle = cell.multiple * decltype(handle_slab)::CAPACITY + index;
			});
			if (ptr->setCHandle(handle) == false) {
				/* The slow path; another thread exposed it before we could and we didn't win the race, so return the cell to the slab. */
				handle_slab.deallocateCell(index);
			}
			return handle;
		} catch(concurrent_slab::NoCellError &) {
			throw ELimitExceeded();
		}
	} else if (obj->isPermanentlyDead()) {
		return 0;
	} else {
		return proposed;
	}
}

void freeCImpl(std::shared_ptr<CExposable> &obj) {
	if (obj->becomePermanentlyDead()) {
		auto handle = obj->getCHandle();
		assert(handle != 0);
		std::size_t cell = handle % decltype(handle_slab)::CAPACITY;
		handle_slab.deallocateCell(cell);
		obj->cDelete();
	}
}

std::shared_ptr<CExposable> getExposableFromHandle(syz_Handle handle) {
	std::size_t index = handle % decltype(handle_slab)::CAPACITY;
	unsigned int multiple = handle / decltype(handle_slab)::CAPACITY;
	std::shared_ptr<CExposable> ret;
	try {
		ret = handle_slab.read(index, [&](std::size_t i, HandleCell &cell) {
			return cell.multiple == multiple ? cell.value : nullptr;
		});
	} catch(concurrent_slab::CellNotAllocatedError &) {
	}
	if (ret == nullptr) {
		throw EInvalidHandle();
	}
	return ret;
}

void clearAllCHandles() {
	handle_slab.deallocateAllCells();
}

/*
 * Infrastructure for deferred frees.
 * */

struct DeferredFreeEntry {
	freeCallback *cb;
	void *value;
};

/*
 * The queue. 1000 items and 64 of each type of producer.
 * */
static moodycamel::ConcurrentQueue<DeferredFreeEntry> deferred_free_queue{1000, 64, 64};
static std::thread deferred_free_thread;
static std::atomic<int> deferred_free_thread_running = 1;
thread_local static bool is_deferred_free_thread = false;

static void deferredFreeWorker() {
	decltype(deferred_free_queue)::consumer_token_t token{deferred_free_queue};
	std::size_t processed = 0;
	is_deferred_free_thread = true;
	while (deferred_free_thread_running.load(std::memory_order_relaxed)) {
		DeferredFreeEntry ent;
		while (deferred_free_queue.try_dequeue(token, ent)) {
			try {
				ent.cb(ent.value);
			} catch(...) {
				logDebug("Exception on memory freeing thread. This should never happen");
			}
			processed++;
		}
		/* Sleep for a bit so that we don't overload the system when we're not freeing. */
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}

	logDebug("Deferred free processed %zu frees in a background thread", processed);
}

void deferredFree(freeCallback *cb, void *value) {
	thread_local decltype(deferred_free_queue)::producer_token_t token{deferred_free_queue};

	if (value == nullptr) {
		return;
	}

	if (is_deferred_free_thread || deferred_free_queue.try_enqueue(token, { cb, value }) == false) {
		cb(value);
	}
}

void initializeMemorySubsystem() {
	deferred_free_thread = std::move(std::thread{deferredFreeWorker});
}

void shutdownMemorySubsystem() {
	deferred_free_thread_running.store(0);
	deferred_free_thread.join();
}

}