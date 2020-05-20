#pragma once

#include "synthizer/error.hpp"
namespace synthizer {

/*
 * Helper functions and macros for implementing the C API.
 * */

/* Infrastructure for setting this thread's last C error code and message. */
void setCThreadError(syz_ErrorCode error, const char *message);

template<typename C>
syz_ErrorCode cWrapper(C &&callable) {
	try {
		return callable();
	} catch (Error &e) {
		setCThreadError(e.cCode(), e.what());
		return e.cCode();
	} catch (...) {
		setCThreadError(1, "Unknown error.");
		return 1;
	}
}

/*
 * Every C function should start with SYZ_PROLOGUE as the very first line, and end with SYZ_EPILOGUE as the very last line. These handle things like wrapping exception handling.
 * Don't do SYZ_PROLOGUE;, it's literally SYZ_PROLOGUE on its own.
 */
#define SYZ_PROLOGUE \
auto _c_wrapper = [&]() -> syz_ErrorCode {

#define SYZ_EPILOGUE \
}; \
return cWrapper(_c_wrapper);

}
