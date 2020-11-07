#include "synthizer/effects/echo.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/error.hpp"

#include <array>
#include <utility>

namespace synthizer {
template<bool FADE_IN, bool ADD>
void EchoEffect::runEffectInternal(float *output, float gain) {
	this->line.runReadLoop(this->max_delay_tap, [&](unsigned int i, auto &reader) {
		float acc_l = 0.0f;
		float acc_r = 0.0f;
		for (auto &t: this->taps) {
			acc_l += reader.read(0, t.config.delay) * t.config.gain_l;
			acc_r += reader.read(1, t.config.delay) * t.config.gain_r;
		}
		if (FADE_IN) {
			float g = i / (float)config::BLOCK_SIZE;
			acc_l *= g;
			acc_r *= g;
		}
		acc_l *= gain;
		acc_r *= gain;
		if (ADD) {
			output[i * 2] += acc_l;
			output[i * 2 + 1] += acc_r;
		} else {
			output[i * 2] = acc_l;
			output[i * 2 + 1] = acc_r;
		}
	});
}

void EchoEffect::runEffect(unsigned int time_in_blocks, unsigned int input_channels, float *input, unsigned int output_channels, float *output, float gain) {
	thread_local std::array<float, config::BLOCK_SIZE * 2> working_buf;

	auto *buffer = this->line.getNextBlock();
	std::fill(buffer, buffer + config::BLOCK_SIZE * 2, 0.0f);
	/* Mix data to stereo. */
	mixChannels(config::BLOCK_SIZE, input, input_channels, buffer, 2);

	/* maybe apply the new config. */
	deferred_vector<EchoTapConfig> new_config;
	bool will_crossfade = this->pending_configs.try_dequeue(new_config);
	/* Drain any stale values. When this is moved to a proper box, this will be handled for us. */
	while (this->pending_configs.try_dequeue(new_config));
	if (will_crossfade) {
		this->taps.clear();
		this->max_delay_tap = 0;
		for (auto &c: new_config) {
			EchoTap tap;
			tap.config = c;
			this->taps.push_back(tap);
			this->max_delay_tap = this->max_delay_tap > c.delay ? this->max_delay_tap : c.delay;
		}
	}

	if (output_channels != 2) {
		if (will_crossfade) {
			this->runEffectInternal<true, false>(&working_buf[0], gain);
		} else {
			this->runEffectInternal<false, false>(&working_buf[0], gain);
		}
		mixChannels(config::BLOCK_SIZE, &working_buf[0], 2, output, output_channels);
	} else {
		if (will_crossfade) {
			this->runEffectInternal<true, true>(output, gain);
		} else {
			this->runEffectInternal<false, true>(output, gain);
		}
	}
}

void EchoEffect::resetEffect() {
	this->line.clear();
}

void EchoEffect::pushNewConfig(deferred_vector<EchoTapConfig> &&config) {
	if (this->pending_configs.enqueue(std::move(config)) == false) {
		throw std::bad_alloc();
	}
}

}

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
