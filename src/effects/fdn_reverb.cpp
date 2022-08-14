#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/effects/fdn_reverb.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/effects/global_effect.hpp"

#include <utility>

using namespace synthizer;

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
