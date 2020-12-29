#pragma once

#include "synthizer/config.hpp"
#include "synthizer/trylock.hpp"

#include <cstdint>

namespace synthizer {

const std::size_t MAX_BLOCK_BUFFER_CACHE_ENTRIES = 16;

/**
 * The BlockBufferCache returns buffers sized to fit one block of audio data, specifically config::BLOCK_SIZE * config::MAX_CHANNELS.
 * 
 * This is used to replace thread_local blocks in lots of places, so that the bound on the number of blocks is effectively
 * the depth of recursion, not how many functions decided to use thread_local.  It also provides safety around cases wherein
 * objects might get called recursively while using thread_local data, such that deeper levels of recursion accidentally override shallower levels.
 * 
 * This is done through a non-movable smart pointer which releases the buffer on destruction and coerces to a float *.
 * */

/**
 * The smart pointer for the cache. Coerces to float * as necessary.
 * */
class BlockBufferGuard {
	public:
	BlockBufferGuard() = delete;
	BlockBufferGuard(const BlockBufferGuard &other) = delete;
	BlockBufferGuard(const BlockBufferGuard &&other) = delete;
	~BlockBufferGuard();

	operator float*() {
		return this->data;
	}

	std::size_t size() {
		return config::BLOCK_SIZE * config::MAX_CHANNELS;
	}

	private:
	explicit BlockBufferGuard(float *data): data(data) {}

	float *data = nullptr;

	friend BlockBufferGuard acquireBlockBuffer(bool should_zero);
};


/**
 * Acquire a block from the cache.
 * 
 * May allocate if the cache overflow.
 * */
	BlockBufferGuard acquireBlockBuffer(bool should_zero = true);
}
