#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/error.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/generators/noise.hpp"

namespace synthizer {

int ExposedNoiseGenerator::getObjectType() {
	return SYZ_OTYPE_NOISE_GENERATOR;
}

void ExposedNoiseGenerator::generateBlock(float *out, FadeDriver *gd) {
	auto working_buf_guard = acquireBlockBuffer();
	float *working_buf_ptr = working_buf_guard;

	std::fill(working_buf_ptr, working_buf_ptr + config::BLOCK_SIZE * this->channels, 0.0f);

	int noise_type;

	if (this->acquireNoiseType(noise_type)) {
		for (auto &g: this->generators) {
			g.setNoiseType(noise_type);
		}
	}

for (unsigned int i = 0; i < this->channels; i++) {
		this->generators[i].generateBlock(config::BLOCK_SIZE, working_buf_ptr + i, this->channels);
	}

	gd->drive(this->getContextRaw()->getBlockTime(), [&] (auto &gain_cb) {
		for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
			float g = gain_cb(i);
			for (unsigned int ch = 0; ch < this->channels; ch++) {
				unsigned int ind = this->channels * i + ch;
				out[ind] += g * working_buf_ptr[ind];
			}
		}
	});
}

std::optional<double> ExposedNoiseGenerator::startGeneratorLingering() {
	setGain(0.0);
	/**
	 * Linger for one block, to give the gain a chance to fade out.
	 * 
	 * This is also sufficient for if the generator is paused.
	 * */
	return config::BLOCK_SIZE / (double)config::SR;
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createNoiseGenerator(syz_Handle *out, syz_Handle context, unsigned int channels, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	if (channels == 0) {
		throw ERange("NoiseGenerator must have at least 1 channel");
	}
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<ExposedNoiseGenerator>(channels);
	*out = toC(x);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}
