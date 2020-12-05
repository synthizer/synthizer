#pragma once

#include "synthizer/at_scope_exit.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/property_internals.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

namespace synthizer {

/*
 * A property ring is sort of a MPSC ringbuffer of a fixed size, which folds the property type into a marker
 * that indicates whether the next cell is available, being written, or enqueued for application.
 * 
 * The ring gets flushed on every wake-up of the audio thread. When full, property setting wakes the audio thread, and waits on the ring to be drained. The ring is made large enough that it never
 * gets full under normal usage.
 * 
 * This is slightly weaker than a queue: order is only guaranteed among properties on the same "thread", where "thread" means that the caller of Synthizer establishes their own happens-before with a mutex etc.
 * practically speaking this should be invisible to end users.
 * */

enum PROPERTY_RING_STATE {
	PROPERTY_RING_EMPTY = 0,
	PROPERTY_RING_ALLOCATED_FOR_WRITE = 1,
	PROPERTY_RING_READY = 2,
};

struct PropertyRingEntry {
	std::atomic<unsigned int> state = PROPERTY_RING_EMPTY;
	int property;
	std::weak_ptr<BaseObject> target;
	property_impl::PropertyValue value;
};

template<std::size_t capacity>
class PropertyRing {
	static_assert(capacity != 0);
	/* Must be power of 2. */
	static_assert((capacity & (capacity - 1)) == 0);

	public:
	/* Returns false if the ring is full. */
	template<typename T>
	bool enqueue(const std::shared_ptr<BaseObject> &obj, int property, T&& value);

	/*
	 * Returns true if a property was applied, otherwise false.
	 * 
	 * To drive the ring, call this in a loop until it returns false.
	 * */
	bool applyNext();

	private:
	std::array<PropertyRingEntry, capacity> ring{};
	std::atomic<std::uint64_t> reader_index = 0;
	std::atomic<std::uint64_t> writer_index = 0;
};

template<std::size_t capacity>
template<typename T>
bool PropertyRing<capacity>::enqueue(const std::shared_ptr<BaseObject> &obj, int property, T&& value) {
	/*
	 * To allocate, we load both pointers, find out if the distance is less than the capacity, then try to increment.
	 * 
	 * From an academic perspective this is incorrect, but it would take well over 5000 years of writing 100m elements per second before either index wraps.
	 * */
	std::size_t index;
	while (1) {
		auto start = this->reader_index.load(std::memory_order_relaxed);
		auto end = this->writer_index.load(std::memory_order_relaxed);
		auto dist = end - start;
		if (dist >= capacity) {
			/* We'd get "behind" the write pointer. */
			return false;
		}
		/* try to mark end as ours. If we can, increment the index. */
		index = end % capacity;
		unsigned int state = PROPERTY_RING_EMPTY;
		if (this->ring[index].state.compare_exchange_strong(state, PROPERTY_RING_ALLOCATED_FOR_WRITE, std::memory_order_acquire)) {
			this->writer_index.fetch_add(1, std::memory_order_relaxed);
			break;
		}
	}

	auto &x = this->ring[index];
	x.property = property;
	x.target = obj;
	x.value = value;
	x.state.store(PROPERTY_RING_READY, std::memory_order_release);

	return true;
}

template<std::size_t capacity>
bool PropertyRing<capacity>::applyNext() {
	std::size_t index = this->reader_index.load(std::memory_order_relaxed) % capacity;

	auto &cell = this->ring[index];
	if (cell.state.load(std::memory_order_acquire) != PROPERTY_RING_READY) {
		return false;
	}

	AtScopeExit cleanup([&] () {
		this->reader_index++;
		cell.state.store(PROPERTY_RING_EMPTY, std::memory_order_release);
	});

	auto target = cell.target.lock();
	if (target == nullptr) {
		return true;
	}

	target->setProperty(cell.property, cell.value);
	return true;
}

}
