#pragma once

#include "synthizer/at_scope_exit.hpp"

#include <atomic>
#include <array>
#include <cassert>
#include <cstdint>
#include <exception>
#include <type_traits>

namespace synthizer::concurrent_slab {

class NoCellError: std::exception {
	const char *what() const override {
		return "No cell left in slab";
	}
};

class CellNotAllocatedError: std::exception {
	const char *what() {
		return "Cell not allocated";
	}
};

/*
 * A concurrent slab allows simultaneous lockfree allocation and deallocation of a set of cells, but doesn't allow
 * concurrent modification.
 * 
 * The slab maintains a free list of cells, initially of every cell in the slab, and pulls from it on allocation.  Every cell in the slab is reference counted;
 * when the reference count of a cell goes to zero, it gets put on the free list.
 * 
 * T must be default constructable and will be constructed once at initialization of the slab. After that, it's the responsiibility of the user to reset the cell
 * on allocation.
 * 
 * For the time being, we limit the slab to 65535 elements so that we can use uint16_t.
 * 
 * Note: the slab leaks cells if the finalizer throws, and the finalizer may be called from any thread, not the one that deallocates.
 * */
template<typename T, typename FINALIZER, std::size_t capacity>
class ConcurrentSlab {
	static_assert(capacity < 65536);

	public:
	static const std::size_t CAPACITY = capacity;

	/*
	 * Associate a callable which gets called on cell deinitialization. It getspassed 9std::size_t, T&) and can mutate T.
	 * */
	ConcurrentSlab(FINALIZER finalizer);
	~ConcurrentSlab();

	/*
	 * Runs the provided closure on the cell, after which the cell is allocated. Returns the return value of the closure.
	 * 
	 * The closure is passed (std::size_t index, T &cell, args...).
	 */
	template<typename CALLABLE, typename... ARGS>
	auto allocateCell(CALLABLE &&callable, ARGS&&... args) -> typename std::invoke_result<CALLABLE, std::size_t, T&, ARGS&&...>::type;

	/*
	 * Given the index of a cell, deallocates the cell. The provided closure
	 * gets to observe the cell after deallocation, but before it's on the free list. This allows for deinitialization as necessary.
	 * */
	void deallocateCell(std::size_t index);
	void deallocateAllCells();

	/*
	 * Observe a cell. Throw CellNotAllocatedError if it's not allocated.
	 * 
	 * The closure can mutate the cell but only in a threadsafe manner.
	 * */
	template<typename CALLABLE, typename... ARGS>
	auto read(std::size_t index, CALLABLE&& callable, ARGS&&... args) -> typename std::invoke_result<CALLABLE, std::size_t, T&, ARGS&&...>::type;

	private:

	/*
	 * Pop a cell from the free list and return its index. Don't increment the reference count.
	 * */
	std::size_t popFreelist();
	/*
	 * Push a cell onto the freelist.
	 * */
	void pushFreelist(std::size_t index);

	/*
	 * Increment/decrement operations. The last decrement for a cell calls the finalizer and puts the cell back on the free list.
	 * 
	 * Returns the reference count after the operation.
	 * */
	std::uint16_t incRef(std::size_t index);
	std::uint16_t decRef(std::size_t index);

	/*
	 * The slab representation is arrays for packing.
	 * */
	/* The cells themselves. */
	std::array<T, capacity> cells{};
	/* The reference counts for the cells. */
	std::array<std::atomic<std::uint16_t>, capacity> refcounts{};
	/* The free list is a linked list implemented as an array. */
	const std::uint16_t FREELIST_SENTINEL = capacity;
	std::array<std::atomic<std::uint16_t>, capacity> freelist{};
	std::atomic<std::uint16_t> freelist_head = FREELIST_SENTINEL;

	FINALIZER finalizer;
};

template<typename T, typename FINALIZER, std::size_t capacity>
ConcurrentSlab<T, FINALIZER, capacity>::ConcurrentSlab(FINALIZER finalizer): finalizer(finalizer) {
	/*
	 * Fill the free list.
	 * 
	 * It's really actually a stack, so doo it in reverse order. This'll make us allocate starting from the front.
	 * */
	std::size_t i = capacity;
	do {
		i--;
		this->pushFreelist(i);
	} while(i);
}

template<typename T, typename FINALIZER, std::size_t capacity>
ConcurrentSlab<T, FINALIZER, capacity>::~ConcurrentSlab() {
	this->deallocateAllCells();
}

/* Fundamental operations. */
template<typename T, typename FINALIZER, std::size_t capacity>
std::uint16_t ConcurrentSlab<T, FINALIZER, capacity>::incRef(std::size_t index) {
	assert(index < capacity);
	while (true) {
		auto old = this->refcounts[index].load(std::memory_order_relaxed);
		/* Fires if we wrap, in debug builds. */
		assert(old < (old + 1));
		if (old == 0) throw CellNotAllocatedError();
		if (this->refcounts[index].compare_exchange_strong(old, old + 1, std::memory_order_acquire)) {
			return old + 1;
		}
	}
}

template<typename T, typename FINALIZER, std::size_t capacity>
std::uint16_t ConcurrentSlab<T, FINALIZER, capacity>::decRef(std::size_t index) {
	assert(index < capacity);
	auto old = this->refcounts[index].fetch_sub(1, std::memory_order_release);
	/* Fires if we wrap. */
	assert(old > (old - 1));
	if (old == 1) {
		this->finalizer(index, this->cells[index]);
		this->pushFreelist(index);
	}
	return old - 1;
}

template<typename T, typename FINALIZER, std::size_t capacity>
std::size_t ConcurrentSlab<T, FINALIZER, capacity>::popFreelist() {
	while (1) {
		auto f_head = this->freelist_head.load(std::memory_order_relaxed);
		if (f_head == FREELIST_SENTINEL) throw NoCellError();
		auto f_next = this->freelist[f_head].load(std::memory_order_relaxed);
		if (this->freelist_head.compare_exchange_strong(f_head, f_next, std::memory_order_acquire)) {
			this->freelist[f_head].store(FREELIST_SENTINEL, std::memory_order_relaxed);
			return f_head;
		}
	}
	/* Unreachable. */
	return 0;
}

template<typename T, typename FINALIZER, std::size_t capacity>
void ConcurrentSlab<T, FINALIZER, capacity>::pushFreelist(std::size_t index) {
	assert(index < capacity);

	while (1) {
		auto f_head = this->freelist_head.load(std::memory_order_relaxed);
		this->freelist[index].store(f_head, std::memory_order_relaxed);
		if (this->freelist_head.compare_exchange_strong(f_head, index, std::memory_order_release)) {
			return;
		}
	}
}

template<typename T, typename FINALIZER, std::size_t capacity>
template<typename CALLABLE, typename... ARGS>
auto ConcurrentSlab<T, FINALIZER, capacity>::allocateCell(CALLABLE &&callable, ARGS&&... args) -> typename std::invoke_result<CALLABLE, std::size_t, T&, ARGS&&...>::type {
	auto index = this->popFreelist();
	/*
	 * This is a horrible hack to deal with incomplete type void, because C++ is annoying and doesn't let you have void variables even though this would be very useful.
	 * We can't store the value of the closure, but need to increment only after it's ran.
	 * Note that copy constructors can throw as well, and there is one involved here.
	 * */
	AtScopeExit x([&]() {
		this->refcounts[index].store(1, std::memory_order_release);
	});
	return callable(index, this->cells[index], args...);
}

template<typename T, typename FINALIZER, std::size_t capacity>
void ConcurrentSlab<T, FINALIZER, capacity>::deallocateCell(std::size_t index) {
	/* There's nothing we can do to protect against double free here, but double free will get caught when a decRef call on some thread asserts that we wrapped. */
	this->decRef(index);
}

/*
 * Todo: a better algorithm. This only works for small-enough slabs that we can iterate over the whole thing in a reasonable amount of time.
 * 
 * The proper way is to maintain a second list of cells that are allocated, but this currently only gets called at library shutdown.
 * */
template<typename T, typename FINALIZER, std::size_t capacity>
void ConcurrentSlab<T, FINALIZER, capacity>::deallocateAllCells() {
	for (std::size_t i = 0; i < capacity; i++) {
		if (this->refcounts[i].load(std::memory_order_relaxed)) {
			this->decRef(i);
		}
	}
}

template<typename T, typename FINALIZER, std::size_t capacity>
template<typename CALLABLE, typename... ARGS>
auto ConcurrentSlab<T, FINALIZER, capacity>::read(std::size_t index, CALLABLE &&callable, ARGS&&... args)  -> typename std::invoke_result<CALLABLE, std::size_t, T&, ARGS&&...>::type {
	auto ref = this->incRef(index);
	AtScopeExit x([&] () { this->decRef(index); });
	/* If we got the first reference it's not allocated. */
	if (ref == 1) throw CellNotAllocatedError();
	return callable(index, this->cells[index], args...);
}

}