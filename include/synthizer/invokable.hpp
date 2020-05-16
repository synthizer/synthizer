#pragma once

#include "synthizer/queues/vyukov.hpp"

#include "sema.h"

#include <type_traits>

namespace synthizer {

/*
 * An Invokable is just a base class for anything providing an invoke method and a VyukovHeader so that it can go onto a queue somewhere.
 * 
 * These are like futures, but intentionally stripped down and intentionally tieing the lifetime of the thing to be invoked to the future, that is disregarding an Invokable before its invocation will result in crashes.
 * */
class Invokable: public VyukovHeader<Invokable> {
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
 * 
 * TODO: this should use a per-thread semaphore.
 * */
template<typename C, bool is_void>
class WaitableInvokable;

template<typename C>
class WaitableInvokable<C, true>: public Invokable {
	public:
	using RESULT_TYPE = typename std::invoke_result<C>::type;

	WaitableInvokable(C &&callable): callable(callable) {}

	void invoke() {
		this->callable();
		this->semaphore.signal();
	}

	RESULT_TYPE wait() {
		this->semaphore.wait();
	}
	private:
	C callable;
	Semaphore semaphore;
};

template<typename C>
class WaitableInvokable<C, false>: public Invokable {
	public:
	using RESULT_TYPE = typename std::invoke_result<C>::type;

	WaitableInvokable(C &&callable): callable(callable) {}

	void invoke() {
		this->result = this->callable();
		this->semaphore.signal();
	}

	RESULT_TYPE wait() {
		this->semaphore.wait();
		return this->result;
	}

	private:
	C callable;
	Semaphore semaphore;
	RESULT_TYPE result;
};

/* Type deduction guide: pick a WaitableInvokable specialization from above. */
template<typename C>
WaitableInvokable(C &&x) -> WaitableInvokable<C, std::is_same<void, typename std::invoke_result<C>::type>::value>;

}