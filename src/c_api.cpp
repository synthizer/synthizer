#include "synthizer.h"

#include "synthizer/audio_output.hpp"
#include "synthizer/background_thread.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"

#include <array>
#include <atomic>
#include <string>
#include <tuple>

namespace synthizer {

/* Error code management. */
thread_local syz_ErrorCode last_error_code = 0;
thread_local std::string last_error_message = "";

void setCThreadError(syz_ErrorCode error, const char *message) {
	last_error_code = error;
	last_error_message = message;
}

/**
 * Set to 0 after the first library initialization. Thereafter, incremented
 * every time a function which requires Synthizer initialization enters the
 * library and decremented when they exit.
 *
 * When the library is uninitialized, set to -2 to prevent double init per
 * process.
 * */
std::atomic<int> is_initialized = -1;

void beginInitializedCall(bool require_init) {
	if (require_init == false) {
		return;
	}

	int expected = is_initialized.load(std::memory_order_relaxed);

	if (expected < 0) {
		throw EUninitialized();
	}

	while (is_initialized.compare_exchange_strong(expected, expected + 1, std::memory_order_relaxed, std::memory_order_relaxed) == false) {
		if (expected < 0) {
			throw EUninitialized();
		}
	}
}

void endInitializedCall(bool require_init) {
	if (require_init == false) {
		return;
	}

	assert(is_initialized.fetch_sub(1, std::memory_order_relaxed) >= 0);
}

}

/* C stuff itself; note we're outside the namespace. */
using namespace synthizer;

SYZ_CAPI void syz_libraryConfigSetDefaults(struct syz_LibraryConfig *cfg) {
	*cfg = syz_LibraryConfig{};
}

SYZ_CAPI syz_ErrorCode syz_initialize(void) {
	struct syz_LibraryConfig cfg;
	syz_libraryConfigSetDefaults(&cfg);
	return syz_initializeWithConfig(&cfg);
}

SYZ_CAPI syz_ErrorCode syz_initializeWithConfig(const struct syz_LibraryConfig *config) {
	SYZ_PROLOGUE_UNINIT

	if (is_initialized.load(std::memory_order_relaxed) != -1) {
		throw Error("Library has already been initialized in this process");
	}

	switch (config->logging_backend) {
	case SYZ_LOGGING_BACKEND_NONE:
		break;
	case SYZ_LOGGING_BACKEND_STDERR:
		logToStderr();
		break;
	default:
		throw ERange("Invalid log_level");
	}

	setLogLevel((enum SYZ_LOG_LEVEL)config->log_level);

	initializeMemorySubsystem();
	startBackgroundThread();
	initializeAudioOutputDevice();
	if (config->libsndfile_path != nullptr) {
		loadLibsndfile(config->libsndfile_path);
	}

	is_initialized.store(0, std::memory_order_relaxed);

	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_shutdown() {
	SYZ_PROLOGUE_UNINIT

	/* Spin until we can uninitialize the library. */
	int expected = 0;
	while (is_initialized.compare_exchange_strong(expected, -2, std::memory_order_relaxed, std::memory_order_relaxed) != false) {
		if (expected < 0) {
			throw EUninitialized();
		}
		expected = 0;
	}

	is_initialized.store(0, std::memory_order_relaxed);

	clearAllCHandles();
	shutdownOutputDevice();
	stopBackgroundThread();
	shutdownMemorySubsystem();
	logDebug("Library shutdown complete");
	return 0;
	SYZ_EPILOGUE
}


SYZ_CAPI syz_ErrorCode syz_getLastErrorCode(void) {
	return last_error_code;
}

SYZ_CAPI const char *syz_getLastErrorMessage(void) {
	return last_error_message.c_str();
}

/*
 * Memory management.
 * */
SYZ_CAPI syz_ErrorCode syz_handleIncRef(syz_Handle handle) {
	SYZ_PROLOGUE_UNINIT

	/**
	 * If the library is uninitialized, all other functions will return an error. In
	 * that case, we just assume that the handle exists from the user's perspective,
	 * which allows languages like Rust to implement infallible cloning.
	 * */
	int expected = is_initialized.load(std::memory_order_relaxed);
	do {
		if (expected < 0) {
			return 0;
		}
	} while (is_initialized.compare_exchange_strong(expected, expected + 1, std::memory_order_relaxed, std::memory_order_relaxed) == false);
	auto _guard = AtScopeExit([]() { is_initialized.fetch_sub(1, std::memory_order_relaxed); });

	auto h = fromC<CExposable>(handle);
	h->incRef();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_handleDecRef(syz_Handle handle) {
	SYZ_PROLOGUE_UNINIT

	/**
	 * If the library is uninitialized, all other functions will return an error. In
	 * that case, we just assume that the handle exists from the user's perspective,
	 * which allows languages like Rust to implement infallible cloning.
	 * */
	int expected = is_initialized.load(std::memory_order_relaxed);
	do {
		if (expected < 0) {
			return 0;
		}
	} while (is_initialized.compare_exchange_strong(expected, expected + 1, std::memory_order_relaxed, std::memory_order_relaxed) == false);
	auto _guard = AtScopeExit([]() { is_initialized.fetch_sub(1, std::memory_order_relaxed); });

	if (handle == 0) {
		return 0;
	}
	auto h = fromC<CExposable>(handle);
	if (h->decRef() == 0) {
		auto bo = std::dynamic_pointer_cast<BaseObject>(h);
		if (bo) {
			bo->getContext()->doLinger(bo);
		}
	}
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_handleGetObjectType(int *out, syz_Handle handle) {
	SYZ_PROLOGUE
	auto obj = fromC<CExposable>(handle);
	*out = obj->getObjectType();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_handleGetUserdata(void **out, syz_Handle handle) {
	SYZ_PROLOGUE
	auto o = fromC<CExposable>(handle);
	*out = o->getUserdata();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_handleSetUserdata(syz_Handle handle, void *userdata, syz_UserdataFreeCallback *free_callback) {
	SYZ_PROLOGUE
	auto o = fromC<CExposable>(handle);
	o->setUserdata(userdata, free_callback);
	return 0;
	SYZ_EPILOGUE
}

/*
 * This function cannot return T directly because this somehow triggers C1001 on MSVC 16.8 and 16.9 when compiling
 * only in release mode for X86.  C1001 is an internal compiler error (read: the compiler crashes), so there's nothing we can do but
 * be inconvenient.
 * 
 * This doesn't have an issue because in practice we can't fix it unless this version of MSVC somehow becomes irrelevant, which is highly unlikely for the mid-to-long-term future.  I encourage maintainers to try not to touch this function at all.
 * */
template<typename T>
static void propertyGetter(T *out, std::shared_ptr<BaseObject> &obj, int property) {
	property_impl::PropertyValue var = obj->getProperty(property);
	T *val = std::get_if<T>(&var);
	if (val == nullptr) {
		throw EPropertyType();
	}
	*out = *val;
}

SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	propertyGetter<int>(out, o, property);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setI(syz_Handle target, int property, int value) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	auto ctx = o->getContextRaw();
	ctx->setIntProperty(o, property, value);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_getD(double *out, syz_Handle target, int property) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	propertyGetter<double>(out, o, property);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setD(syz_Handle target, int property, double value) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	auto ctx = o->getContextRaw();
	ctx->setDoubleProperty(o, property, value);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	auto ctx = o->getContextRaw();
	std::shared_ptr<CExposable> val = nullptr;
	if (value) {
		val = fromC<CExposable>(value);
	}
	ctx->setObjectProperty(o, property, val);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_getD3(double *x, double *y, double *z, syz_Handle target, int property) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	std::array<double, 3> val;
	propertyGetter<std::array<double, 3>>(&val, o, property);
	*x = val[0];
	*y = val[1];
	*z = val[2];
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	auto ctx = o->getContextRaw();
	ctx->setDouble3Property(o, property, {x, y, z});
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_getD6(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2, syz_Handle target, int property) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	std::array<double, 6> val;
	propertyGetter<std::array<double, 6>>(&val, o, property);
	*x1 = val[0];
	*y1 = val[1];
	*z1 = val[2];
	*x2 = val[3];
	*y2 = val[4];
	*z2 = val[5];
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setD6(syz_Handle target, int property, double x1, double y1, double z1, double x2, double y2, double z2) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	auto ctx = o->getContextRaw();
	ctx->setDouble6Property(o, property, { x1, y1, z1, x2, y2, z2 });
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_getBiquad(struct syz_BiquadConfig *filter, syz_Handle target, int property) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	propertyGetter<syz_BiquadConfig>(filter, o, property);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setBiquad(syz_Handle target, int property, const struct syz_BiquadConfig *filter) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	auto ctx = o->getContextRaw();
	ctx->setBiquadProperty(o, property, *filter);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI void syz_initDeleteBehaviorConfig(struct syz_DeleteBehaviorConfig *cfg) {
	*cfg = syz_DeleteBehaviorConfig{};
}

SYZ_CAPI syz_ErrorCode syz_configDeleteBehavior(syz_Handle object, const struct syz_DeleteBehaviorConfig *cfg) {
	SYZ_PROLOGUE
	auto obj = fromC<BaseObject>(object);
	obj->setDeleteBehaviorConfig(*cfg);
	return 0;
	SYZ_EPILOGUE
}
