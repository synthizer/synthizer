#pragma once

#include <atomic>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <tuple>
#include <thread>

#include "sema.h"

#include "config.hpp"
#include "memory.hpp"
#include "types.hpp"

namespace synthizer {
static_assert(std::atomic<std::size_t>::is_always_lock_free, "Unable to use mutex in size_t atomic inside AudioRing due to kernel calls.");

/*
 * This is currently unused logic for a ringbuffer which is intended to hold audio data.
 * 
 * Unfortunately, it turned out that miniaudio isn't able to tell us what the maximum size of this buffer should be, so we had to switch to an alternative per-block setup. See bounded_block_queue.hpp.
 * 
 * Keeping this around because it's useful, just not for the problem at hand.
 * */

/* Number of times you have to advance begin to get to end. */
inline std::size_t ringDist(std::size_t begin, std::size_t end, std::size_t size) {
	return (end + size - begin) % size;
}

/* 
 * An inline AudioSample ringbuffer, which supports waking when space is available.
 * 
 * Note that this is SPSC. SPMC usage is not safe.
 * 
 * On the producer side, this ringbuffer always returns aligned chunks of data: in debug builds allocateForWrite will assert if the space requested isn't a multiple of config::ALIGNMENT.
 * 
 * For convenience, also contains a mechanism for counting the number of times the write side got less than it requested, which can be used for the purposes of increasing/decreasing latency targets.
 * 
 * You want the types at the end of this file, which specify static/dynamic ringbuffers and provide public constructors.
 * */
template<typename data_provider_t>
class AudioRingBase {
	public:
	const std::size_t MIN_ADVANCE = config::ALIGNMENT / sizeof(AudioSample);

	/*
	 * This is modelled after DirectSound.
	 * 
	 * The first pointer is always set to a non-null value.  The second pointer might be non-null, if the request wraps around the end of the buffer.
	 * 
	 * Note that if the user of this function always requests the same size on every call, that size is a factor of the ring size, and the user always fully commits, the second pointer is never non-NULL and the first pointer is the entire block.
	 * 
	 * In particular, the latter functionality is used by the audio threads. We support nonb-multiples for file I/O and generators.
	 * 
	 * If maxAvailable is true, return at least the size requested, but if more space is available then return all the space.
	 * 
	 * If maxAvailablen is true, size is zero, and immediate is true, never block and return what is available. In this case, it is possible to get 0/nullptr for all 4 values if what is available is less than MIN_ADVANCE, and you shouldn't call endWrite if you do.
	 * */
	std::tuple<std::size_t, AudioSample *, std::size_t, AudioSample *>
	beginWrite(std::size_t size, bool immediate = false, bool maxAvailable = false) {
		/* Fail if we're not requesting a multiple of aligned data. */
		assert(size % MIN_ADVANCE == 0);
		assert(maxAvailable == true || size != 0);
		/* What if we requested a size that's bigger than the buffer? */
		assert(size <= this->size());

		/*
		 * Explanation: the write pointer is always "behind" the read pointer, such that if write == read, then there is no data in the buffer.
		 * */
		std::size_t available, read_pointer, write_pointer;
		write_pointer = this->write_pointer.load(std::memory_order_relaxed);
		do {
			read_pointer = this->read_pointer.load(std::memory_order_relaxed);
			/* Get the number of bytes left, subtract from the size of the ring. */
			available = this->size() - ringDist(read_pointer, write_pointer, this->size());
			if (immediate)
				break;
			if (available < size)
				this->read_end_sema.wait();
		} while ( available < size);

		if (available < MIN_ADVANCE)
			return {0, nullptr, 0, nullptr};

		std::size_t size1 = 0, size2 = 0;
		std::size_t allocating = maxAvailable ? available : std::min(size, available);
		this->pending_write_size = allocating;

		size1 = std::min(this->size() - write_pointer, allocating);
		if (size1 == allocating)
			return { size1, &this->data_provider[0] + write_pointer, 0, nullptr };

		size2 = allocating - size1;
		return { size1, &this->data_provider[0] + write_pointer, size2, &this->data_provider[0] };
	}

	void endWrite(std::size_t amount) {
		assert(amount <= this->pending_write_size);
		std::size_t wp = this->write_pointer.load(std::memory_order_relaxed);
		wp = (wp + amount) % this->size();
		this->write_pointer.store(wp, std::memory_order_release);
		this->pending_write_size -= amount;
	}

	void endWrite() {
		endWrite(this->pending_write_size);
	}

	/*
	 * The read side. Like the write side.
	 * Doesn't inforce buffer alignment.
	 * */
	std::tuple<std::size_t, AudioSample *, std::size_t, AudioSample *>
	beginRead(std::size_t size, bool immediate = false, bool maxAvailable = false) {
		assert(maxAvailable == false || size != 0);
		/* What if we requested a size that's bigger than the buffer? */
		assert(size <= this->size());

		std::size_t available, read_pointer, write_pointer;
		read_pointer = this->read_pointer.load(std::memory_order_relaxed);
		do {
			write_pointer = this->write_pointer.load(std::memory_order_relaxed);
			/* Unlike write, it's exactly the number of advances. */
			available = ringDist(read_pointer, write_pointer, this->size());
			if (immediate)
				break;
			if (available > size)
				std::this_thread::yield();
		} while ( available < size);

		if (available == 0)
			return {0, nullptr, 0, nullptr};

		std::size_t allocating = maxAvailable ? available : std::min(size, available);
		this->pending_read_size = allocating;

		std::size_t size1 = std::min(allocating, this->size() - read_pointer);
		AudioSample *ptr1 = &this->data_provider[0] + read_pointer;
		if (size1 == allocating)
			return {size1, ptr1, 0, nullptr};

		std::size_t size2 = allocating - size1;
		AudioSample *ptr2 = &this->data_provider[0];
		return {size1, ptr1, size2, ptr2};
	}

	void endRead(std::size_t amount) {
		assert(amount <= this->pending_read_size);

		auto rp = this->read_pointer.load(std::memory_order_relaxed);
		rp = (rp + amount) % this->size();
		this->read_pointer.store(rp);
		this->pending_read_size -= amount;
		this->read_end_sema.signal();
	}

	void endRead() {
		endRead(this->pending_read_size);
	}

	/* Increment the late counter. */
	void signalLate() {
		this->late_counter.fetch_add(1, std::memory_order_relaxed);
	}

	/* Fetch and reset the late counter. */
	unsigned int getAndResetLate() {
		return this->late_counter.store(0);
	}

	std::size_t size() {
		return this->data_provider.size();
	}

	protected:
	AudioRingBase() = default;

	data_provider_t data_provider;
	std::atomic<std::size_t> write_pointer = 0, read_pointer = 0;
	std::atomic<unsigned int> late_counter = 0;
	std::size_t pending_write_size = 0, pending_read_size = 0;
	Semaphore read_end_sema;
};

template<std::size_t n>
class InlineAudioRingProvider {
	public:
	InlineAudioRingProvider() {
		this->data.fill(0.0f);
	}

	constexpr std::size_t size() const {
		return this->data.size();
	}

	AudioSample &
	operator[](std::size_t x) {
		return this->data[x];
	}

	private:
	alignas(config::ALIGNMENT) std::array<AudioSample, n> data;
};

/* An inline allocated audio ring. */
template<std::size_t size>
class InlineAudioRing: public AudioRingBase<InlineAudioRingProvider<size>> {
	public:

	static_assert(size > 0 && size % (config::ALIGNMENT / sizeof(AudioSample)) == 0, "Size must be alignable.");

	InlineAudioRing() {
	}
};

class AllocatedRingProvider {
	public:

	~AllocatedRingProvider() {
		freeAligned(this->data);
	}
	
	std::size_t size() const {
		return this->_size;
	}

	void allocate(std::size_t n) {
		this->data = allocAligned<AudioSample>(n);
		this->_size = n;
		std::fill(this->data, this->data+this->_size, 0.0f);
	}

	AudioSample&
	operator[](std::size_t x) {
		return *(this->data+x);
	}

	private:
	std::size_t _size = 0;
	AudioSample *data = nullptr;
};

/* An allocated (heap) ring. */
class AllocatedAudioRing: public AudioRingBase<AllocatedRingProvider> {
	public:
	AllocatedAudioRing(std::size_t n) {
		assert(n > 0 && n % (config::ALIGNMENT / sizeof(AudioSample)) == 0);
		this->data_provider.allocate(n);
	}
};

}