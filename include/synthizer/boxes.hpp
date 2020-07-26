#pragma once

#include <atomic>
#include <limits>

namespace synthizer {

/*
 * boxes are used to send values between threads, so that things such as properties can be set without the overhead of queues where possible. An example of this usage is
 * the Decoding generator, which needs to be able to send position and looping to a background thread.
 * 
 * In general the receiver calls read, which returns true if there's an update that no oneb has observed, and the sender calls
 * write, which puts the update in the box.
 * */

/*
 * The InvalidValueBox uses the CRTP to indicate an invallid value that will never be put into the box,
 * and uses it to tell whether an update is pending. This box is MPMC, except that each update can be observed by at most one thread.
 * 
 * This will work on any type that can be used with constexpr, but should probably only be used on things smaller than primitives,
 * since larger types aren't going to be lockfree.
 * */
template<typename T, typename INVAL_PROVIDER>
class InvalidValueBox: INVAL_PROVIDER {
	public:
	InvalidValueBox(T initial) {
		this->value.store(initial, std::memory_order_relaxed);
	}

	/*
	 * Read from the box. Out is untouched if this returns false.
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

/* Typedef for a box that can hold 0 or 1. */
using BoolBox = InvalidValueBox<int, TemplateInvalidProvider<int, 2>>;
/* Typedef for a box that can hold any finite double. */
using FiniteDoubleBox = InvalidValueBox<double, InvalidInfiniteDouble>;

}