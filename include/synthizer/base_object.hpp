#pragma once

namespace synthizer {

/*
 * The ultimate base class for all Synthizer objects which are to be exposed to the public, 
 * 
 * Synthizer uses dynamic_cast<Object(value) plus some other magic to convert handles into objects, with runtime type checking.
 * 
 * To use this properly, make sure that you virtual inherit from this directly or indirectly if adding a new object type.
 * */
class BaseObject {
	virtual ~BaseObject() {}
};

}