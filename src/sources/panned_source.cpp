#include "synthizer.h"

#include "synthizer/sources.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace synthizer {

PannedSource::PannedSource(const std::shared_ptr<Context> &context): context(context) {
	this->panner_lane = context->allocateSourcePannerLane(this->panner_strategy);
}

double PannedSource::getAzimuth() {
	return this->azimuth;
}

void PannedSource::setAzimuth(double azimuth) {
	this->azimuth = azimuth;
	this->is_scalar_panning = false;
	this->needs_panner_set = true;
}

double PannedSource::getElevation() {
	return this->elevation;
}

void PannedSource::setElevation(double elevation) {
	this->elevation = elevation;
	this->is_scalar_panning = false;
	this->needs_panner_set = true;
}

double PannedSource::getPanningScalar() {
	return this->panning_scalar;
}

void PannedSource::setPanningScalar(double panning_scalar) {
	this->panning_scalar = panning_scalar;
	this->is_scalar_panning = true;
	this->needs_panner_set = true;
}

int PannedSource::getPannerStrategy() {
	return this->panner_strategy;
}

void PannedSource::setPannerStrategy(int strategy) {
	this->panner_lane = nullptr;
	this->panner_strategy = (enum SYZ_PANNER_STRATEGIES) strategy;
}

double PannedSource::getGain() {
	return this->gain;
}

void PannedSource::setGain(double gain) {
	this->gain = gain;
}

void PannedSource::run() {
	alignas(config::ALIGNMENT) static thread_local std::array<AudioSample, config::BLOCK_SIZE * config::MAX_CHANNELS> multichannel_buffer_array;
	alignas(config::ALIGNMENT) static thread_local std::array<AudioSample, config::BLOCK_SIZE> mono_buffer_array;
	AudioSample *mono_buffer = &mono_buffer_array[0];
	AudioSample *multichannel_buffer = &multichannel_buffer_array[0];

	std::fill(mono_buffer, mono_buffer + config::BLOCK_SIZE, 0.0f);

	if (this->panner_lane == nullptr) {
		this->panner_lane = this->context->allocateSourcePannerLane(this->panner_strategy);
		this->needs_panner_set = true;
	}

	if (this->needs_panner_set == true) {
		if (this->is_scalar_panning == true) {
			this->panner_lane->setPanningScalar(this->panning_scalar);
		} else {
			this->panner_lane->setPanningAngles(this->azimuth, this->elevation);
		}
		this->needs_panner_set = false;
	}

	/* iterate and remove as we go to avoid locking weak_ptr twice. */
	weak_vector::iterate_removing(this->generators, [&] (auto &g) {
		unsigned int nch = g->getChannels();

		if (nch == 0) {
			return;
		}

		if (nch == 1) {
			/* Fast path: straight to mono_buffer. */
			g->generateBlock(mono_buffer);
		} else {
			/* Write to multichannel_buffer, then fold the first n channels normalized by 1/n. */
			std::fill(multichannel_buffer, multichannel_buffer + config::BLOCK_SIZE * nch, 0.0f);
			g->generateBlock(multichannel_buffer);

			float normfactor = 1.0f / nch;
			for (unsigned int c = 0; c < nch; c++) {
				for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
					float s = multichannel_buffer[nch * i + c];
					mono_buffer[i] += s * normfactor;
				}
			}
		}
	});

	/* And output. */
	this->panner_lane->update();
	unsigned int stride = this->panner_lane->stride;
	AudioSample *dest = this->panner_lane->destination;
	float g = this->gain;
	for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
		dest[i * stride] = g * mono_buffer[i];
	}
}

}

/* Do properties. */
#define PROPERTY_CLASS PannedSource
#define PROPERTY_LIST PANNED_SOURCE_PROPERTIES
#define PROPERTY_BASE BaseObject
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createPannedSource(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto ret = ctx->createObject<PannedSource>(ctx);
	*out = toC(ret);
	return 0;
	SYZ_EPILOGUE
}
