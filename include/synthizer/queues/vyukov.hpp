#pragma once

#include <atomic>
#include <cassert>

namespace synthizer {

template<typename T>
class VyukovQueue;

/* An intrucive Vyukov queue using inheritance.
 *
 * This is a MPSC lockfree queue, without support for waiting.
 * */

/* Use CRTP. */
template<typename T>
class VyukovHeader {
	friend class VyukovQueue<T>;

	private:
	std::atomic<VyukovHeader<T> *> next = nullptr;
};

/* The queue itself. */
template<typename T>
class VyukovQueue {
	public:
	VyukovQueue();

	/* The single consumer side.
	 * 
	 * Unlike a normal Vyukov queue, this spin waits when it detects that someone else is in the middle of enqueueing
	 * This reduces the number of false nullptrs under high contention, but may not be necessary in practice.
	 * */
	T* dequeue();
	/* The multiple producer side. */
	void enqueue(T *item);

	private:
	void enqueueHeader(VyukovHeader<T> *header);
	std::atomic<VyukovHeader<T>*> head = nullptr, tail = nullptr;
	VyukovHeader<T> sentinel;
};

template<typename T>
VyukovQueue<T>::VyukovQueue() {
	this->head.store(&this->sentinel);
	this->tail.store(&this->sentinel);
}

template<typename T>
void VyukovQueue<T>::enqueueHeader(VyukovHeader<T> *header) {
	/* Still ours. Relaxed memory order is fine. */
	header->next.store(nullptr, std::memory_order_relaxed);
	/* We're publishing a tail to other threads, and also trying to read next values other threads may have written, so ack_rel. */
	auto t = this->tail.exchange(header, std::memory_order_acq_rel);
	/* Now, we're releasing ours to the consumer thread. */
	t->next.store(header, std::memory_order_release);
}

template<typename T>
void VyukovQueue<T>::enqueue(T *item) {
	enqueueHeader(item);
}

template<typename T>
T* VyukovQueue<T>::dequeue() {
	VyukovHeader<T> *result = nullptr;
	// this unwinds a recursive implementation.
	// This can be faster by hoisting loads to locals, but for now I've gone for clarity.
	for(;;) {
		// Always non-null, either the sentinel or otherwise.
		auto h = this->head.load(std::memory_order_acquire);
		// But this could be null.
		auto n = h->next.load(std::memory_order_acquire);
		if (h == &this->sentinel && n == nullptr) {
			// h is the sentinel, but there's no next, so nothing we can do.
			break;
		} else if(h == &this->sentinel) {
			// We are on the sentinel, but we have a next, so we can bump the head forward and try again.
			assert(n != &this->sentinel); // Should never come back to sentinel from the sentinel.
			this->head.store(n, std::memory_order_relaxed);
			continue;
		} else if(n == nullptr) {
			// h wasn't the sentinel, but there's no next.
			// We can fix this though, by just enqueueing the sentinel and skipping it again later.
			// When this call returns, we definitely have a next.
			this->enqueueHeader(&this->sentinel);
			// But that next might not be the sentinel, if someone else was enqueueing.
			this->head.store(h->next.load(std::memory_order_acquire), std::memory_order_relaxed);
			result = h;
			break;
		} else {
			// h isn't the sentinel, and we do hav a next.
			this->head.store(n, std::memory_order_relaxed);
			result = h;
			break;
		}
	}
	return static_cast<T*>(result);
}

}