#pragma once

#include "synthizer/audio_ring.hpp"
#include "synthizer/config.hpp"

#include "autoresetevent.h"

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
 * This wraps a closure and an audio ring to make that happen.
 * 
 * CALLABLE is called as (channels, buffer) and should output config::BLOCK_SIZE worth of data.
 * Channels is always as specified in the constructor, but passed for convenience.
 * 
 * start/stop shouldn't be called concurrently by different threads or from the callable.
 * */
class GenerationThread {
	public:
	/*
	 * max_latency must be > 0 and a multiple of config::BLOCK_SIZE.
	 * 
	 * To actually start the thread, call start with your closure.
	 * */
	GenerationThread(std::size_t channels, std::size_t max_latency): channels(channels),
		ring(max_latency * channels) {
		assert(max_latency > 0);
		assert(max_latency % config::BLOCK_SIZE == 0);
	}

	~GenerationThread() {
		this->stop();
	}

	template<typename CALLABLE>
	void start(CALLABLE &&callable);
	void stop();

	/*
	 * Reads frames. Returns the amount actually read.
	 * */
	std::size_t read(std::size_t amount, AudioSample *dest);

	/*
	 * used for pitch bend, etc. Skips frames in the buffer.
	 * */
	std::size_t skip(std::size_t amount);


	template<typename CALLABLE>
	void backgroundThread(CALLABLE&& callable);

	private:
	std::size_t channels;
	AllocatedAudioRing ring;
	std::atomic<int> running = 0;
	std::thread thread;
};

template<typename CALLABLE>
void GenerationThread::start(CALLABLE &&callable) {
	this->running.store(1);
	this->thread = std::move(std::thread([=] () {
		this->backgroundThread(callable);
	}));
}

void GenerationThread::stop() {
	auto expected = this->running.load();
	if (expected && this->running.compare_exchange_strong(	expected, 0)) {
		/* Drain the ring, which will wake up the background thread and let it know that it's time to stop. */
		this->ring.beginRead(0, true);
		this->ring.endRead();
		this->thread.join();
	}
}

std::size_t GenerationThread::read(std::size_t amount, AudioSample *dest) {
	auto [size1, ptr1, size2, ptr2] = this->ring.beginRead(amount * this->channels);
	assert(size1 % this->channels == 0);
	assert(size2 % this->channels == 0);
	std::copy(ptr1, ptr1 + size1 * this->channels, dest);
	if (ptr2) {
		std::copy(ptr2, ptr2 + size2 * this->channels, dest + size1 * this->channels);
	}
	this->ring.endRead();
	return size1 + size2;
}

std::size_t GenerationThread::skip(std::size_t amount) {
	auto [size1, ptr1, size2, ptr2] = this->ring.beginRead(amount * this->channels, true);
	this->ring.endRead();
	assert(size1 % channels == 0);
	assert(size2 % channels == 0);
	return size1 + size2;
}

template<typename CALLABLE>
void GenerationThread::backgroundThread(CALLABLE &&callable) {
	while (this->running.load(std::memory_order_relaxed)) {
		auto [size1, ptr1, size2, ptr2] = this->ring.beginWrite(config::BLOCK_SIZE * this->channels);
		assert(size2 == 0 && ptr2 == nullptr);
		assert(size1 == config::BLOCK_SIZE * this->channels);
		callable(this->channels, ptr1);
		this->ring.endWrite();
	}
}


}