#pragma once

#include <atomic>
#include <thread>

namespace synthizer {

enum class ThreadsafeInitializerStates: int {
	uninitialized = 0,
	initializing = 1,
	initialized = 2,
};

/*
 * A wrapper for something that we'd like in a global variable, but whose constructor might throw.
 * 
 * This class provides a .get() function which returns a T&, such that the first call to .get() will initialize the T& in a threadsafe fashion, throwing any errors it might throw.
 * */
template<typename T>
class ThreadsafeInitializer {
	public:
	T& get() {
		/* The fast path. */
		if (state.load() == (int)ThreadsafeInitializerStates::initialized)
			return *data;

		/* Slow path. Try to become the initializer. */
		int expected = (int)ThreadsafeInitializerStates::uninitialized;
		/* exactly one thread will succeed at running the following line, so no loop is necessary. */
		if (this->state.compare_exchange_strong(expected, (int)ThreadsafeInitializerStates::initializing)) {
			this->data = new T();
			this->state.store((int)ThreadsafeInitializerStates::initialized);
			return *this->data;
		}

		/* Otherwise, do a simple spinlock. */
		while (this->state.load() != (int)ThreadsafeInitializerStates::initialized)
			std::this_thread::yield();
		return *this->data;
	}

	private:
		T *data;
		std::atomic<int> state{(int)ThreadsafeInitializerStates::uninitialized};
};

}