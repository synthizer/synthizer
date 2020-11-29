#include "synthizer/effects/fdn_reverb.hpp"

#include "synthizer_properties.h"

#include "synthizer/c_api.hpp"

#define PROPERTY_CLASS GlobalFdnReverbEffect
#define PROPERTY_LIST FDN_REVERB_EFFECT_PROPERTIES
#define PROPERTY_BASE BaseObject
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<GlobalFdnReverbEffect>(1);
	*out = toC(x);
	std::shared_ptr<GlobalEffectBase> e = x;
	ctx->registerGlobalEffect(	e);
	return 0;
	SYZ_EPILOGUE
}
