#include "synthizer/buffer.hpp"

#include "synthizer_constants.h"

#include "synthizer/byte_stream.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoders/raw_decoder.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include "synthizer/math.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/stream_handle.hpp"

#include "WDL/resample.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

using namespace synthizer;

/**
 * Part of the C API only. Create a buffer from a decoder and return the handle.
 * */
static syz_Handle bufferFromDecoder(std::shared_ptr<AudioDecoder> dec) {
  auto data = bufferDataFromDecoder(dec);
  auto buf = allocateSharedDeferred<Buffer>(data);
  auto ce = std::static_pointer_cast<CExposable>(buf);
  ce->stashInternalReference(ce);
  return toC(buf);
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamParams(syz_Handle *out, const char *protocol, const char *path,
                                                        void *param, void *userdata,
                                                        syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  auto dec = getDecoderForStreamParams(protocol, path, param);
  *out = bufferFromDecoder(dec);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromEncodedData(syz_Handle *out, unsigned long long data_len, const char *data,
                                                       void *userdata,
                                                       syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  auto stream = memoryStream(data_len, data);
  auto dec = getDecoderForStream(stream);
  *out = bufferFromDecoder(dec);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromFloatArray(syz_Handle *out, unsigned int sr, unsigned int channels,
                                                      unsigned long long frames, const float *data, void *userdata,
                                                      syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  if (channels > config::MAX_CHANNELS) {
    throw ERange("Too many channels");
  }
  auto dec = getRawDecoder(sr, channels, frames, data);
  *out = bufferFromDecoder(dec);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromFile(syz_Handle *out, const char *path, void *userdata,
                                                syz_UserdataFreeCallback *userdata_free_callback) {
  return syz_createBufferFromStreamParams(out, "file", path, NULL, userdata, userdata_free_callback);
}

SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamHandle(syz_Handle *out, syz_Handle stream, void *userdata,
                                                        syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  auto s = fromC<StreamHandle>(stream);
  auto bs = consumeStreamHandle(s);
  auto dec = getDecoderForStream(bs);
  *out = bufferFromDecoder(dec);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetChannels(unsigned int *out, syz_Handle buffer) {
  SYZ_PROLOGUE
  auto b = fromC<Buffer>(buffer);
  *out = b->getChannels();
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSamples(unsigned int *out, syz_Handle buffer) {
  SYZ_PROLOGUE
  auto b = fromC<Buffer>(buffer);
  *out = b->getLength();
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSeconds(double *out, syz_Handle buffer) {
  SYZ_PROLOGUE
  auto b = fromC<Buffer>(buffer);
  *out = b->getLength() / (double)config::SR;
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_bufferGetSizeInBytes(unsigned long long *size, syz_Handle buffer) {
  SYZ_PROLOGUE
  auto buf = fromC<Buffer>(buffer);
  *size = buf->getLength() * buf->getChannels() * 2;
  return 0;
  SYZ_EPILOGUE
}
