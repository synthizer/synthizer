#include "synthizer.h"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/memory.hpp"

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
