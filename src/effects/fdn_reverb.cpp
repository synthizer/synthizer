#include "synthizer/effects/fdn_reverb.hpp"

#include "synthizer/property_xmacros.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/effects/global_effect.hpp"


#define PROPERTY_CLASS FdnReverbEffect<GlobalEffect>
#define PROPERTY_CLASS_IS_TEMPLATE
#define PROPERTY_LIST FDN_REVERB_EFFECT_PROPERTIES
#define PROPERTY_BASE GlobalEffect
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<FdnReverbEffect<GlobalEffect>>();
	std::shared_ptr<GlobalEffect> e = x;
	ctx->registerGlobalEffect(	e);
	*out = toC(x);
	return 0;
	SYZ_EPILOGUE
}
