#pragma once

#include "synthizer/spsc_semaphore.hpp"

#include <exception>
#include <type_traits>
#include <utility>

namespace synthizer {

/*
 * An Invokable is just a base class for anything providing an invoke method.
 * 
 * These are like futures, but intentionally stripped down and intentionally tieing the lifetime of the thing to be invoked to the future, that is disregarding an Invokable before its invocation will result in crashes.
 * */
class Invokable {
	public:
	virtual ~Invokable() {}
	virtual void invoke() = 0;
};

/*
 * AN Invokable that wraps a callable, throwing out the result.
 * */
template<typename T>
class CallableInvokable: public Invokable {
	public:
	CallableInvokable(T &&x): callable(x) {}

	void invoke() {
		this->callable();
	}

	private:
	T callable;
};

/*
 * An invokable that has a wait method which will return the result.
 * */
template<typename C, bool is_void>
class WaitableInvokable;

template<typename C>
class WaitableInvokable<C, true>: public Invokable {
	public:
	using RESULT_TYPE = typename std::invoke_result<C>::type;

	WaitableInvokable(C &&callable): callable(std::move(callable)) {}

	void invoke() {
		try {
			this->callable();
		} catch(...) {
			this->exception = std::current_exception();
		}

		this->semaphore.signal();
	}

	RESULT_TYPE wait() {
		this->semaphore.wait();
		if (this->exception) std::rethrow_exception(this->exception);
	}

	private:
	C callable;
	SPSCSemaphore semaphore;
	std::exception_ptr exception;
};

template<typename C>
class WaitableInvokable<C, false>: public Invokable {
	public:
	using RESULT_TYPE = typename std::invoke_result<C>::type;

	WaitableInvokable(C &&callable): callable(std::move(callable)) {}

	void invoke() {
		try {
			this->result = this->callable();
		} catch(...) {
			this->exception = std::current_exception();
		}
		this->semaphore.signal();
	}

	RESULT_TYPE wait() {
		this->semaphore.wait();
		if (this->exception) std::rethrow_exception(this->exception);
		return this->result;
	}

	private:
	C callable;
	SPSCSemaphore semaphore;
	RESULT_TYPE result;
	std::exception_ptr exception;
};

/* Type deduction guide: pick a WaitableInvokable specialization from above. */
template<typename C>
WaitableInvokable(C &&x) -> WaitableInvokable<C, std::is_same<void, typename std::invoke_result<C>::type>::value>;

}