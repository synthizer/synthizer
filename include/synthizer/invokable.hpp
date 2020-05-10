#pragma once

#include "synthizer/queues/vyukov.hpp"

namespace synthizer {

/*
 * An Invokable is just a base class for anything providing an invoke method and a VyukovHeader so that it can go onto a queue somewhere.
 * 
 * These are like futures, but intentionally stripped down and intentionally tieing the lifetime of the thing to be invoked to the future, that is disregarding an Invokable before its invocation will result in crashes.
 * */
class Invokable: VyukobHeader<Invokable> {
	public:
	virtual ~Invokable() {}
	virtual invoke() = 0;
};

/*
 * AN Invokable that wraps a callable, throwing out the result.
 * */
template<typename T>
class CallableInvokable: public Invokable {
	public:
	CallableInvokable(T &&x): callable(x) {}

	invoke() {
		this->callable();
	}

	private:
	T callable;
};

}