#pragma once
#include "synthizer.h"

#include "synthizer/error.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/property_internals.hpp"

#include <atomic>

namespace synthizer {

class Context;

/*
 * The ultimate base class for all Synthizer objects which are to be exposed to the public, 
 * 
 * To use this properly, make sure that you virtual inherit from this directly or indirectly if adding a new object type.
 * */
class BaseObject: public CExposable {
	public:
	BaseObject(const std::shared_ptr<Context> &ctx): context(ctx) {}
	virtual ~BaseObject() {}

	/* A baseObject has no properties whatsoever. */
	virtual bool hasProperty(int property) {
		return false;
	}


	virtual property_impl::PropertyValue getProperty(int property) {
		throw EInvalidProperty();
	}

	virtual void validateProperty(int property, const property_impl::PropertyValue &value) {
		throw EInvalidProperty();
	}

	virtual void setProperty(int property, const property_impl::PropertyValue &value) {
		throw EInvalidProperty();
	}

	/* Virtual because context itself needs to override to always return itself. */
	virtual std::shared_ptr<Context> getContext() {
		return this->context;
	}

	/* From the C API implementations, you usually want this one. */
	virtual Context *getContextRaw() {
		return this->context.get();
	}

	protected:
	std::shared_ptr<Context> context;
};

}