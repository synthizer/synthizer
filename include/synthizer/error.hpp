#pragma once
#include "synthizer.h"

#include <string>
#include <exception>

namespace synthizer {

	/*
	 * Base class for all synthizer errors.
	 * 
	 * When Synthizer encounters an error, it throws an instance of this class,  which will be translated by the C API into error codes and messages.
	 * 
	 * Since we don't have a C API yet, right now this doesn't have an error code.
	 * */
class Error: public std::exception  {
	public:
	Error(std::string message);
	virtual const std::string &getMessage() const;
	virtual ~Error() {}

	const char *what() const noexcept { return this->message.c_str(); }
	/* Get the code for the C API. Will differentiate later. */
	syz_ErrorCode cCode() { return 1; }

	template<typename T>
	bool is() {
		return dynamic_cast<T*>(this) != nullptr;
	}

	template<typename T>
	T* as() {
		auto out = dynamic_cast<T>(this);
		return out;
	}

	private:
	std::string message;
};

#define ERRDEF3(type, default_msg, base) \
class type: public base { \
	public: \
	type(std::string msg = default_msg): base(msg) {} \
}
#define ERRDEF(type, default_msg) ERRDEF3(type, default_msg, Error)

/* Some module agnostic errors. */
ERRDEF(EUninitialized, "The library is not initialized.");

ERRDEF(ELimitExceeded, "Internal limit exceeded");
ERRDEF(EInvalidHandle, "C handle is invalid");
ERRDEF(EHandleType, "Handle of the wrong type provided");
ERRDEF(EInvalidProperty, "Not a valid property for this object type");
ERRDEF(EPropertyType, "Property type mismatch");
ERRDEF(ERange, "Value out of range");
ERRDEF(EInvariant, "Invariant would be violated");
ERRDEF(EValidation, "Validation error");
ERRDEF(EInternal, "Internal library error");

}