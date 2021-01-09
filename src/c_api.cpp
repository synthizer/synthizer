#include "synthizer.h"

#include "synthizer/audio_output.hpp"
#include "synthizer/background_thread.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"

#include <array>
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

}

/* C stuff itself; note we're outside the namespace. */
using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_configureLoggingBackend(enum SYZ_LOGGING_BACKEND backend, void *param) {
	SYZ_PROLOGUE
	switch (backend) {
	case SYZ_LOGGING_BACKEND_STDERR:
		logToStderr();
		break;
	}
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_initialize() {
	SYZ_PROLOGUE
	initializeMemorySubsystem();
	startBackgroundThread();
	initializeAudioOutputDevice();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_shutdown() {
	SYZ_PROLOGUE
	clearAllCHandles();
	shutdownOutputDevice();
	stopBackgroundThread();
	shutdownMemorySubsystem();
	logDebug("Library shutdown complete");
	return 0;
	SYZ_EPILOGUE
}


SYZ_CAPI syz_ErrorCode syz_getLastErrorCode() {
	return last_error_code;
}

SYZ_CAPI const char *syz_getLastErrorMessage() {
	return last_error_message.c_str();
}

/*
 * Memory management.
 * */
SYZ_CAPI syz_ErrorCode syz_handleFree(syz_Handle handle) {
	SYZ_PROLOGUE
	if (handle == 0) {
		return 0;
	}
	auto h = fromC<CExposable>(handle);
	freeC(h);
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

SYZ_CAPI syz_ErrorCode syz_getUserdata(void **out, syz_Handle handle) {
	SYZ_PROLOGUE
	auto o = fromC<CExposable>(handle);
	*out = o->getUserdata();
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_setUserdata(syz_Handle handle, void *userdata, syz_UserdataFreeCallback *free_callback) {
	SYZ_PROLOGUE
	auto o = fromC<CExposable>(handle);
	o->setUserdata(userdata, free_callback);
	return 0;
	SYZ_EPILOGUE
}

template<typename T>
static T propertyGetter(std::shared_ptr<BaseObject> &obj, int property) {
	auto var = obj->getProperty(property);
	auto val = std::get_if<T>(&var);
	if (val == nullptr) {
		throw EPropertyType();
	}
	return *val;
}

SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property) {
	SYZ_PROLOGUE
	auto o = fromC<BaseObject>(target);
	*out = propertyGetter<int>(o, property);
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
	*out = propertyGetter<double>(o, property);
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
	auto val = propertyGetter<std::array<double, 3>>(o, property);
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
	auto val = propertyGetter<std::array<double, 6>>(o, property);
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
