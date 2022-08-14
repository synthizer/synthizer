#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/sources/direct_source.hpp"
#include "synthizer/types.hpp"

#include <algorithm>
#include <array>

namespace synthizer {

int DirectSource::getObjectType() { return SYZ_OTYPE_DIRECT_SOURCE; }

void DirectSource::run(unsigned int out_channels, float *out) {
  /*
   * For now, use the fact that we know that the context is always stereo.
   * In future this'll need to be done better.
   * */
  assert(out_channels == 2);
  this->fillBlock(out_channels);

  for (unsigned int i = 0; i < config::BLOCK_SIZE * out_channels; i++) {
    out[i] += this->block[i];
  }
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createDirectSource(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                              syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE(void) config;

  auto ctx = fromC<Context>(context);
  auto ret = ctx->createObject<DirectSource>();
  ctx->registerSource(ret);
  *out = toC(ret);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
