#pragma once

#include "synthizer/queues/vyukov.hpp"

#include "sema.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cassert>

namespace synthizer {

/*
 * A MPSC priority queue with a static number of priorities.
 * 
 * Items in the queue need to inherit from VyukovHeader<T> and will be inserted into internal queues (and consequently shouldn't be in any others at the time).
 * 
 * The implementation uses a semaphore for whenever a new item arrives, and a number of internal VyukovQueues and counters for each level.
 * 
 * In order to account for a future in which we might want to make this queue dynamicly sized without raising interesting questions, the highest priority is 0 and the lowest is PRIORITIES - 1.
 * */
template<typename T, unsigned int PRIORITIES>
class PriorityQueue {
	public:
	T *dequeue() {
		while(true) {
			for(auto l: this->levels) {
				if (l.count.load(std::memory_order_relaxed)) {
					T* item;
					do {
						item = l.queue.dequeue();
					} while(item == nullptr);
					l.count.sub(1, std::memory_order_relaxed);
					return item;
				}
			}
			this->semaphore.wait();
		}
		return nullptr;
	}

	void enqueue(T *item, unsigned int priority) {
		assert(priority < PRIORITIES);
		this->priorities[priority].queue.enqueue(item);
		this->priorities[priority].count.fetch_add(1, std::memory_order_relaxed);
		this->semaphore.signal();
	}

	private:
	struct Level {
		std::atomic<unsigned int> count;
		VyukovQueue<T> queue;
	};

	std::array<Level, PRIORITIES> levels;
	Semaphore semaphore;
};

}