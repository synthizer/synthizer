#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/sources.hpp"
#include "synthizer/types.hpp"

#include <algorithm>
#include <array>

namespace synthizer {

int DirectSource::getObjectType() { return SYZ_OTYPE_DIRECT_SOURCE; }

void DirectSource::run() {
  /*
   * For now, use the fact that we know that the context is always stereo.
   * In future this'll need to be done better.
   * */
  this->fillBlock(2);
  float *buf = this->context->getDirectBuffer();
  for (unsigned int i = 0; i < config::BLOCK_SIZE * 2; i++) {
    buf[i] += this->block[i];
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
