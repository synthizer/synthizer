#include "synthizer/effects/echo.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/error.hpp"

#define PROPERTY_CLASS GlobalEchoEffect
#define PROPERTY_LIST EFFECT_PROPERTIES
#define PROPERTY_BASE BaseObject
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createGlobalEcho(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<GlobalEchoEffect>(1);
	*out = toC(x);
	std::shared_ptr<GlobalEffectBase> e = x;
	ctx->registerGlobalEffect(	e);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_echoSetTaps(syz_Handle handle, unsigned int n_taps, struct syz_EchoTapConfig *taps) {
	SYZ_PROLOGUE
	deferred_vector<EchoTapConfig> cfg;
	auto echo = fromC<EchoEffect>(handle);
 cfg.reserve(n_taps);
	for (unsigned int i = 0; i < n_taps; i++) {
		const unsigned int delay_in_samples = taps[i].delay * config::SR;
		if (delay_in_samples > EchoEffect::MAX_DELAY) {
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
