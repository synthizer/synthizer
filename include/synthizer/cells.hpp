#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace synthizer {

/*
 * Cells are used to send values between threads, so that things such as properties can be set without the overhead of queues where possible. An example of this usage is
 * the Decoding generator, which needs to be able to send position and looping to a background thread.
 * 
 * In general the receiver calls read, which returns true if there's an update that no oneb has observed, and the sender calls
 * write, which puts the update in the box.
 * */

/*
 * The InvalidValueCell uses the CRTP to indicate an invallid value that will never be put into the cell,
 * and uses it to tell whether an update is pending. This box is MPMC, except that each update can be observed by at most one thread.
 * 
 * This will work on any type that can be used with constexpr, but should probably only be used on things smaller than primitives,
 * since larger types aren't going to be lockfree.
 * */
template<typename T, typename INVAL_PROVIDER>
class InvalidValueCell: INVAL_PROVIDER {
	public:
	InvalidValueCell(T initial) {
		this->value.store(initial, std::memory_order_relaxed);
	}

	/*
	 * Read from the cell. Out is untouched if this returns false.
	 * */
	bool read(T* out) {
		T tmp;
		tmp = this->value.load(std::memory_order_relaxed);
		while (true) {
			if (tmp == this->getInvalid()) {
				return false;
			}
			if (this->value.compare_exchange_strong(tmp, this->getInvalid(), std::memory_order_acquire, std::memory_order_relaxed)) {
				*out = tmp;
				return true;
			}
		}
	}

	void write(T value) {
		this->value.store(value, std::memory_order_release);
	}

	private:
	std::atomic<T> value;
};

template<typename T, T INVAL>
class TemplateInvalidProvider {
	protected:
	T getInvalid() {
		return INVAL;
	}
};

class InvalidInfiniteDouble {
	protected:
	double getInvalid() {
		return std::numeric_limits<double>::infinity();
	}
};

/* Typedef for a cell that can hold 0 or 1. */
using BoolCell = InvalidValueCell<int, TemplateInvalidProvider<int, 2>>;
/* Typedef for a cell that can hold any finite double. */
using FiniteDoubleCell = InvalidValueCell<double, InvalidInfiniteDouble>;

/*
 * A LatchedScell is an SPMC cell modelled after the Linux kernel. Writers never block, but readers
 * have some latency.
 * 
 * This works by having two internal objects, which are copies of each other.  When a writer wants to write, it directs the reader to the other object.
 * 
 * Internally, the cell consists of two objects in an array and a version counter.  When a writer writes, it increments the version counter,
 * uses the low bit to decide which value to write to, then increments the version counter again. When a reader reads, it grabs the current copy of the version counter,
 * uses the low bit to determine where the cell is writing, and grabs the opposite value.
 * 
 * Readers know that they received a consistent snapshot if the version number is the same at the end of their read.
 * This means that readers can perform partial reads in the middle of execution, but that the final value will be consistent.
 * 
 * It is not safe to use this with C++ objects that don't satisfy std::is_trivial, which is checked at
 * compile time.  This is because readers may perform partial reads.
 * */
template<typename T>
class LatchCell {
	static_assert(std::is_trivial<T>::value, "LatchCell may only be used with trivial types because partial reads are possible");

	public:
	LatchCell(): LatchCell(T()) {}

	explicit LatchCell(const T &default_value) {
		this->data[0] = default_value;
		this->data[1] = default_value;
	}

	void write(const T &value) {
		this->writeWithCallback([&](T &destination) {
			destination = value;
		});
	}

	/*
	 * Public for testing, which wants to be able to do the write in a callback that simulates
	 * interrupted writers, which intentionally tear their values.
	 * */
	template<typename CB_T>
	void writeWithCallback(CB_T &&cb) {
		this->version_counter.fetch_add(1, std::memory_order_release);
		cb(this->data[0]);
		this->version_counter.fetch_add(1, std::memory_order_release);
		cb(this->data[1]);
	}

	T read() {
		int v;
		T out;
		do {
			v = this->version_counter.load(std::memory_order_acquire);
			int index = v & 0x01;
			out = this->data[index];
		} while (v != this->version_counter.load(std::memory_order_relaxed));
		return out;
	}

	private:
	std::atomic<int> version_counter = 0;
	std::array<T, 2> data;
};

}
