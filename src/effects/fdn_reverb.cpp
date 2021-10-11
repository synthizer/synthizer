#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/effects/fdn_reverb.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/effects/global_effect.hpp"

#include <utility>

namespace synthizer {

class ExposedGlobalFdnReverb : public FdnReverbEffect<GlobalEffect> {
public:
  template <typename... ARGS>
  ExposedGlobalFdnReverb(ARGS &&... args) : FdnReverbEffect<GlobalEffect>(std::forward<ARGS>(args)...) {}

  int getObjectType() override { return SYZ_OTYPE_GLOBAL_FDN_REVERB; }
};

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto x = ctx->createObject<ExposedGlobalFdnReverb>();
  std::shared_ptr<GlobalEffect> e = x;
  ctx->registerGlobalEffect(e);
  *out = toC(x);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
