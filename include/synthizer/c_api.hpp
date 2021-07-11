#pragma once

#include "synthizer/error.hpp"
#include "synthizer/at_scope_exit.hpp"

namespace synthizer
{

	/*
 * Helper functions and macros for implementing the C API.
 * */

/* Infrastructure for setting this thread's last C error code and message. */
void setCThreadError(syz_ErrorCode error, const char *message);

	template <typename C>
	syz_ErrorCode cWrapper(C &&callable)
	{
		try
		{
			return callable();
		}
		catch (Error &e)
		{
			setCThreadError(e.cCode(), e.what());
			return e.cCode();
		}
		catch (...)
		{
			setCThreadError(1, "Unknown error.");
			return 1;
		}
	}

/**
 * Functions to bracket a call which will ensure that Synthizer is initialized
 * and can't deinitialize while a caller is using it.
 * */
void beginInitializedCall(bool require_init);
void endInitializedCall(bool require_init);

/**
 * version of SYZ_PROLOGUE for functions that should work without the library being initialized.
 * */
#define SYZ_PROLOGUE_ICHECK(NEEDS_INIT) \
	beginInitializedCall((NEEDS_INIT)); \
	auto _init_guard = AtScopeExit([](){ endInitializedCall((NEEDS_INIT)); }); \
	auto _c_wrapper = [&]() -> syz_ErrorCode {

#define SYZ_PROLOGUE_UNINIT SYZ_PROLOGUE_ICHECK(false)

/*
 * Every C function should start with SYZ_PROLOGUE as the very first line, and end with SYZ_EPILOGUE as the very last line. These handle things like wrapping exception handling.
 * Don't do SYZ_PROLOGUE;, it's literally SYZ_PROLOGUE on its own.
 */
#define SYZ_PROLOGUE \
	SYZ_PROLOGUE_ICHECK(true)

#define SYZ_EPILOGUE \
	}                \
	;                \
	return cWrapper(_c_wrapper);

}
