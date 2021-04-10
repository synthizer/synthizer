#pragma once

#include "synthizer/spsc_ring.hpp"
#include "synthizer/config.hpp"

#include "autoresetevent.h"
#include <concurrentqueue.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <thread>
#include <utility>
#include <tuple>

namespace synthizer {

/*
 * Some things need to be able to generate audio in a background thread as fast as possible because they're expensive or would block or etc.
 * 
 * The GenerationThread is fed by calling send, and read from by calling receive.
 * To use it, call start with a closure taking a T*, process the T*, and return.
 * Pass Ts to the thread with send, and get them back with receive. Receive returns false
 * if no item could be dequeued. Configure leadin if you need to make sure a specific number of items have been processed
 * before returning any.
 *
 * start/stop shouldn't be called concurrently by different threads or from the callable.
 * */
template<typename T>
class GenerationThread {
	public:
	/**
	 * if leadin is non-zero, the GenerationThread won't return any data until at least leadin items are processed. This is used by e.g. StreamingGenerator
	 * to make sure that there's enough audio to prevent underruns.
	 * */
	GenerationThread(unsigned int leadin = 0): leadin(leadin) {}

	~GenerationThread() {
		this->stop();
	}

	template<typename CALLABLE>
	void start(CALLABLE &&callable);
	void stop();
	void send(const T &item);
	/**
	 * Returns true if an item was received.
	 * */
	bool receive(T *item);

	private:
	template<typename CALLABLE>
	void backgroundThread(CALLABLE&& callable);

	moodycamel::ConcurrentQueue<T> incoming_queue, outgoing_queue;
	std::atomic<int> running = 0, leadin = 0;
	AutoResetEvent incoming_event;
	std::thread thread;
};

template<typename T>
template<typename CALLABLE>
void GenerationThread<T>::start(CALLABLE &&callable) {
	this->running.store(1);
	this->thread = std::thread([=] () {
		this->backgroundThread(callable);
	});
}

template<typename T>
void GenerationThread<T>::stop() {
	auto expected = this->running.load();
	if (expected && this->running.compare_exchange_strong(	expected, 0)) {
		this->incoming_event.signal();
		this->thread.join();
	}
}

template<typename T>
void GenerationThread<T>::send(const T &item) {
	this->incoming_queue.enqueue(item);
	this->incoming_event.signal();
}

template<typename T>
bool GenerationThread<T>::receive(T *item) {
	if (this->leadin.load(std::memory_order_relaxed) != 0) {
		return false;
	}
	return this->outgoing_queue.try_dequeue(*item);
}

template<typename T>
template<typename CALLABLE>
void GenerationThread<T>::backgroundThread(CALLABLE &&callable) {
	while (this->running.load(std::memory_order_relaxed)) {
		T item;

		if (this->incoming_queue.try_dequeue(item)) {
			callable(&item);
			this->outgoing_queue.enqueue(item);
			if (this->leadin.load(std::memory_order_relaxed) > 0) {
				this->leadin.fetch_sub(1, std::memory_order_relaxed);
			}
		} else {
			this->incoming_event.wait();
		}
	}
}

}
