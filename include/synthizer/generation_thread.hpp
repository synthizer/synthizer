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
	 * 
	 * leadin_latency is how much needs to be computed before the first time we start returning data. This is used to allow buffers to fill enough that there won't be drop-outs.
	 * */
	GenerationThread(std::size_t channels, std::size_t max_latency): GenerationThread(channels, max_latency, max_latency) {}
	GenerationThread(std::size_t channels, std::size_t max_latency, std::size_t leadin_latency): channels(channels), leadin_latency(leadin_latency),
		ring(max_latency * channels) {
		assert(max_latency > 0);
		assert(max_latency % config::BLOCK_SIZE == 0);
		assert(leadin_latency <= max_latency);
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
	std::size_t read(std::size_t amount, float *dest);

	/*
	 * used for pitch bend, etc. Skips frames in the buffer.
	 * */
	std::size_t skip(std::size_t amount);

	template<typename CALLABLE>
	void backgroundThread(CALLABLE&& callable);

	private:
	std::size_t channels;
	std::size_t leadin_latency = 0;
	AllocatedAudioRing ring;
	std::atomic<int> running = 0, leadin_complete = 0;
	std::thread thread;
};

template<typename CALLABLE>
void GenerationThread::start(CALLABLE &&callable) {
	this->leadin_complete.store(0);
	this->running.store(1);
	this->thread = std::thread([=] () {
		this->backgroundThread(callable);
	});
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

std::size_t GenerationThread::read(std::size_t amount, float *dest) {
	if (this->leadin_complete.load(std::memory_order_relaxed) == 0) {
		return 0;
	}

	auto [size1, ptr1, size2, ptr2] = this->ring.beginRead(amount * this->channels);
	/*
	 * Since we request a whole block, size1 == 0 means none was ready
	 * We could request partial blocks, but doing so would just make underruns worse.
	 * */
	if (size1 == 0) {
		this->ring.endRead();
		return 0;
	}

	assert(size1 % this->channels == 0);
	assert(size2 % this->channels == 0);
	assert(size1 + size2 == amount * this->channels);
	std::copy(ptr1, ptr1 + size1, dest);
	if (ptr2) {
		std::copy(ptr2, ptr2 + size2, dest + size1);
	}
	this->ring.endRead();
	return (size1 + size2) / this->channels;
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
	std::size_t frames_generated = 0;
	while (this->running.load(std::memory_order_relaxed)) {
		auto [size1, ptr1, size2, ptr2] = this->ring.beginWrite(config::BLOCK_SIZE * this->channels);
		assert(size2 == 0 && ptr2 == nullptr);
		assert(size1 == config::BLOCK_SIZE * this->channels);
		callable(this->channels, ptr1);
		this->ring.endWrite();
		frames_generated += config::BLOCK_SIZE;
		if (frames_generated >= this->leadin_latency) {
			this->leadin_complete.store(1, std::memory_order_relaxed);
		}
	}
}

}
