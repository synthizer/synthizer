#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/stream_handle.hpp"

#include <memory>

namespace synthizer {

void StreamHandleBase::markConsumed() {
	/* technically the user can re-consume a handle with 2**32 calls that try to do it.  Don't worry about that case and avoid a cas loop. */
	if (this->consumed.fetch_add(1, std::memory_order_relaxed) != 0) {
		throw EValidation("Cannot use StreamHandle twice");
	}
}

int StreamHandleBase::getObjectType() {
	return SYZ_OTYPE_STREAM_HANDLE;
}

std::shared_ptr<ByteStream> consumeStreamHandle(const std::shared_ptr<StreamHandle> &handle) {
	handle->markConsumed();
	return std::static_pointer_cast<ByteStream>(handle);
}

}



using namespace synthizer;

syz_Handle exposeStream(std::shared_ptr<ByteStream> &stream) {
	auto handle = sharedPtrDeferred<StreamHandle>(new StreamHandle(stream), deferredDelete<StreamHandle>);
	auto base = std::static_pointer_cast<CExposable>(handle);
	base->stashInternalReference(base);
	return toC(handle);
}

SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromStreamParams(syz_Handle *out, const char *protocol, const char *path, void *param) {
	SYZ_PROLOGUE
	auto stream = getStreamForStreamParams(protocol, path, param);
	*out = exposeStream(stream);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromMemory(syz_Handle *out, unsigned long long data_len, const char *data) {
	SYZ_PROLOGUE
	auto stream = memoryStream(data_len, data);
	*out = exposeStream(stream);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromFile(syz_Handle *out, const char *path) {
	return syz_createStreamHandleFromStreamParams(out, "file", path, NULL);
}


SYZ_CAPI syz_ErrorCode syz_streamHandleFromCustomStream(syz_Handle *out, struct syz_CustomStreamDef callbacks) {
	SYZ_PROLOGUE
	auto s = customStream(&callbacks);
	*out = exposeStream(s);
	return 0;
	SYZ_EPILOGUE
}
