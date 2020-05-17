#pragma once

#include "synthizer/error.hpp"
#include "synthizer/property_internals.hpp"

namespace synthizer {

/*
 * The ultimate base class for all Synthizer objects which are to be exposed to the public, 
 * 
 * Synthizer uses dynamic_cast<Object(value) plus some other magic to convert handles into objects, with runtime type checking.
 * 
 * To use this properly, make sure that you virtual inherit from this directly or indirectly if adding a new object type.
 * */
class BaseObject {
	public:
	virtual ~BaseObject() {}

	/* A baseObject has no properties whatsoever. */
	virtual bool hasProperty(int property) {
		return false;
	}

	virtual property_impl::PropertyValue getProperty(int property) {
		throw EInvalidProperty();
	}

	/* And getting and setting them immediately throws. */
	virtual void setProperty(int property, const property_impl::PropertyValue &value) {
		throw EInvalidProperty();
	}
};

}