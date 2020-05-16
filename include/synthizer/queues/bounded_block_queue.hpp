#pragma once

#include <atomic>
#include <cstddef>

#include "sema.h"

#include "synthizer/memory.hpp"
#include "synthizer/queues/vyukov.hpp"

namespace synthizer {

/*
 * A bounded SPSC queue, which stores data in blocks.
 * 
 * The blocks of the queue are allocated on either config::ALIGNMENT or alignof(T), whichever is greater.
 * 
 * It is not possible to partially fill blocks, or anything along those lines.
 * */
template<typename T>
class BoundedBlockQueue {
	public:
	BoundedBlockQueue(std::size_t block_size, std::size_t queue_size = 3);
	~BoundedBlockQueue();

	std::size_t getBlockSize();

	/*
	 * Begin a write.
	 * 
	 * Will wait on a block to be available.
	 * 
	 * Entirely fill the block, then call endWrite.
	 * */
	T* beginWrite();
	void endWrite();

	/*
	 * begin a read.
	 * 
	 * Doesn't wait on data. Returns nullptr immediately.
	 * */
	T* beginReadImmediate();
	void endRead();

	/*
	 * Set the size of the queue. Must be at least 1, and queues cannot shrink.
	 * 
	 * This won't immediately allocate pages. The write side of the queue will do that if necessary.
	 * */
	void setQueueSize(std::size_t size);
	std::size_t getQueueSize();

	/* Set whether writes block until a slot is available. This controls wheher or not an internal semaphore increments. */
	void setWritesWillBlock(bool blocking);

private:
	struct Block: public VyukovHeader<Block> {
		T *data;
	};

	VyukovQueue<Block> ready_queue, free_queue;
	std::atomic<std::size_t> queue_size = 1, queue_allocated = 0;
	std::size_t block_size;
	/* Signals reads being completed. */
	Semaphore read_sema;
	Block *read_block = nullptr;
	Block *write_block = nullptr;
	std::atomic<int> writes_will_block = 1;

	void allocateBlocksIfNeeded();
};

template<typename T>
BoundedBlockQueue<T>::BoundedBlockQueue(std::size_t block_size, std::size_t queue_size): block_size(block_size) {
	this->queue_size.store(queue_size, std::memory_order_relaxed);
	allocateBlocksIfNeeded();
}

template<typename T>
BoundedBlockQueue<T>::~BoundedBlockQueue() {
	BoundedBlockQueue<T>::Block *blk;
	while((blk = this->ready_queue.dequeue())) {
		freeAligned(blk->data);
		delete blk;
	}
	while ((blk = this->free_queue.dequeue())) {
		freeAligned(blk->data);
		delete blk;
	}
}

template<typename T>
void BoundedBlockQueue<T>::allocateBlocksIfNeeded() {
	auto needed = this->queue_size.load(std::memory_order_relaxed) - this->queue_allocated.load(std::memory_order_relaxed);
	while (needed) {
		auto block_data = allocAligned<T>(this->block_size);
		auto block = new BoundedBlockQueue<T>::Block();
		block->data = block_data;
		this->free_queue.enqueue(block);
		needed --;
		this->queue_allocated.fetch_add(1);
		this->read_sema.signal();
	}
}

template<typename T>
T* BoundedBlockQueue<T>::beginWrite() {
	assert(this->write_block == nullptr);

	bool will_block = this->writes_will_block.load(std::memory_order_relaxed);

	this->allocateBlocksIfNeeded();
	if (will_block) this->read_sema.wait();

	BoundedBlockQueue<T>::Block *b = nullptr;
	do {
		b = this->free_queue.dequeue();
		if (b) break;
	} while(will_block && b == nullptr);

	this->write_block = b;
	return b ? b->data : nullptr;
}

template<typename T>
void BoundedBlockQueue<T>::endWrite() {
	assert(this->write_block);
	this->ready_queue.enqueue(this->write_block);
	this->write_block = nullptr;
}

template<typename T>
T* BoundedBlockQueue<T>::beginReadImmediate() {
	assert(this->read_block == nullptr);
	auto blk = this->ready_queue.dequeue();
	if (blk == nullptr) {
		return nullptr;
	}
	this->read_block = blk;
	return blk->data;
}

template<typename T>
void BoundedBlockQueue<T>::endRead() {
	assert(this->read_block);
	this->free_queue.enqueue(this->read_block);
	this->read_block = nullptr;
	this->read_sema.signal();
}

template<typename T>
std::size_t BoundedBlockQueue<T>::getBlockSize() {
	return this->block_size;
}

template<typename T>
void BoundedBlockQueue<T>::setQueueSize(std::size_t size) {
	/* We need a CAS loop because it's possible that someone might do this concurrently and the queue must never shrink. */
	auto cur_size = this->queue_size.load(std::memory_order_relaxed);
	for(;;) {
		// Nothing to do.
		if (cur_size >= size) return;
		if (this->queue_size.compare_exchange_weak(&cur_size, size, std::memory_order_relaxed, std::memory_order_relaxed))
			break;
	}
}

template<typename T>
std::size_t BoundedBlockQueue<T>::getQueueSize() {
	return this->queue_size.load(std::memory_order_relaxed);
}

template<typename T>
void BoundedBlockQueue<T>::setWritesWillBlock(bool blocking) {
	this->writes_will_block.store(blocking, std::memory_order_relaxed);
}
}