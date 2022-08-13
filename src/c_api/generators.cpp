#include "synthizer.h"

#include "synthizer/audio_output.hpp"
#include "synthizer/background_thread.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/generators/buffer.hpp"
#include "synthizer/generators/fast_sine_bank.hpp"
#include "synthizer/generators/noise.hpp"
#include "synthizer/generators/streaming.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/stream_handle.hpp"

#include <array>
#include <atomic>
#include <string>
#include <tuple>

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<BufferGenerator>();
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI void syz_initSineBankConfig(struct syz_SineBankConfig *cfg) { *cfg = syz_SineBankConfig{}; }

SYZ_CAPI syz_ErrorCode syz_createFastSineBankGenerator(syz_Handle *out, syz_Handle context,
                                                       const struct syz_SineBankConfig *bank_config, void *config,
                                                       void *userdata,
                                                       syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;
  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<FastSineBankGenerator>(bank_config);
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorSine(syz_Handle *out, syz_Handle context,
                                                           double initial_frequency, void *config, void *userdata,
                                                           syz_UserdataFreeCallback *userdata_free_callback) {
  static const struct syz_SineBankWave wave { 1.0, 0.0, 1.0 };
  struct syz_SineBankConfig cfg;

  cfg.waves = &wave;
  cfg.wave_count = 1;
  cfg.initial_frequency = initial_frequency;
  return syz_createFastSineBankGenerator(out, context, &cfg, config, userdata, userdata_free_callback);
}

static syz_ErrorCode createSineBankFromVec(syz_Handle *out, syz_Handle context, double initial_frequency,
                                           std::vector<syz_SineBankWave> *waves, void *userdata,
                                           syz_UserdataFreeCallback *userdata_free_callback) {
  struct syz_SineBankConfig cfg;

  cfg.waves = &(*waves)[0];
  cfg.wave_count = waves->size();
  cfg.initial_frequency = initial_frequency;
  return syz_createFastSineBankGenerator(out, context, &cfg, NULL, userdata, userdata_free_callback);
}

SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorSquare(syz_Handle *out, syz_Handle context,
                                                             double initial_frequency, unsigned int partials,
                                                             void *config, void *userdata,
                                                             syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;
  auto waves = sb_construction_helpers::buildSquareSeries(partials);
  return createSineBankFromVec(out, context, initial_frequency, &waves, userdata, userdata_free_callback);

  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorTriangle(syz_Handle *out, syz_Handle context,
                                                               double initial_frequency, unsigned int partials,
                                                               void *config, void *userdata,
                                                               syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;
  auto waves = sb_construction_helpers::buildTriangleSeries(partials);
  return createSineBankFromVec(out, context, initial_frequency, &waves, userdata, userdata_free_callback);

  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorSaw(syz_Handle *out, syz_Handle context, double initial_frequency,
                                                          unsigned int partials, void *config, void *userdata,
                                                          syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;
  auto waves = sb_construction_helpers::buildSawtoothSeries(partials);
  return createSineBankFromVec(out, context, initial_frequency, &waves, userdata, userdata_free_callback);

  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createNoiseGenerator(syz_Handle *out, syz_Handle context, unsigned int channels,
                                                void *config, void *userdata,
                                                syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  if (channels == 0) {
    throw ERange("NoiseGenerator must have at least 1 channel");
  }
  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<ExposedNoiseGenerator>(channels);
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamParams(syz_Handle *out, syz_Handle context,
                                                                    const char *protocol, const char *path, void *param,
                                                                    void *config, void *userdata,
                                                                    syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto decoder = getDecoderForStreamParams(protocol, path, param);
  auto generator = ctx->createObject<StreamingGenerator>(decoder);
  *out = toC(generator);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromFile(syz_Handle *out, syz_Handle context, const char *path,
                                                            void *config, void *userdata,
                                                            syz_UserdataFreeCallback *userdata_free_callback) {
  return syz_createStreamingGeneratorFromStreamParams(out, context, "file", path, NULL, config, userdata,
                                                      userdata_free_callback);
}

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamHandle(syz_Handle *out, syz_Handle context,
                                                                    syz_Handle stream, void *config, void *userdata,
                                                                    syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto s = fromC<StreamHandle>(stream);
  auto bs = consumeStreamHandle(s);
  auto decoder = getDecoderForStream(bs);
  auto generator = ctx->createObject<StreamingGenerator>(decoder);
  *out = toC(generator);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
