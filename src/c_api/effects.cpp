#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/effects/echo.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/effects/fdn_reverb.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/error.hpp"

#include <utility>

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_effectReset(syz_Handle effect) {
  SYZ_PROLOGUE
  auto e = fromC<BaseObject>(effect);
  auto e_as_effect = fromC<BaseEffect>(effect);
  Context *ctx = e->getContextRaw();
  ctx->enqueueReferencingCallbackCommand(
      true, [](auto &e_as_effect) { e_as_effect->resetEffect(); }, e_as_effect);
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createGlobalEcho(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                            syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<GlobalEchoEffect>();
  std::shared_ptr<GlobalEffect> e = x;
  ctx->registerGlobalEffect(e);
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_globalEchoSetTaps(syz_Handle handle, unsigned int n_taps,
                                             const struct syz_EchoTapConfig *taps) {
  SYZ_PROLOGUE
  deferred_vector<EchoTapConfig> cfg;
  auto echo = fromC<GlobalEchoEffect>(handle);
  cfg.reserve(n_taps);
  for (unsigned int i = 0; i < n_taps; i++) {
    const unsigned int delay_in_samples = taps[i].delay * config::SR;
    if (delay_in_samples > ECHO_MAX_DELAY) {
      throw ERange("Delay is too long");
    }
    EchoTapConfig c;
    c.delay = delay_in_samples;
    c.gain_l = taps[i].gain_l;
    c.gain_r = taps[i].gain_r;
    cfg.emplace_back(std::move(c));
  }
  echo->pushNewConfig(std::move(cfg));
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<GlobalFdnReverbEffect>();
  std::shared_ptr<GlobalEffect> e = x;
  ctx->registerGlobalEffect(e);
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
