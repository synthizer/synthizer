#include "synthizer.h"

#include "synthizer/sources.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace synthizer {

PannedSource::PannedSource(const std::shared_ptr<Context> context): Source(context) {
}

void PannedSource::initInAudioThread() {
	this->panner_lane = context->allocateSourcePannerLane(this->panner_strategy);
	this->valid_lane = true;
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
	this->valid_lane = false;
	this->panner_strategy = (enum SYZ_PANNER_STRATEGY) strategy;
}

void PannedSource::setGain3D(double gain) {
	this->gain_3d = gain;
}

void PannedSource::run() {
	if (this->valid_lane == false) {
		this->panner_lane = this->context->allocateSourcePannerLane(this->panner_strategy);
		this->needs_panner_set = true;
		this->valid_lane = true;
	}

	if (this->needs_panner_set == true) {
		if (this->is_scalar_panning == true) {
			this->panner_lane->setPanningScalar(this->panning_scalar);
		} else {
			this->panner_lane->setPanningAngles(this->azimuth, this->elevation);
		}
		this->needs_panner_set = false;
	}

	this->fillBlock(1);

	/* And output. */
	this->panner_lane->update();
	unsigned int stride = this->panner_lane->stride;
	AudioSample *dest = this->panner_lane->destination;
	float g = this->gain_3d;
	for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
		dest[i * stride] = g * block[i];
	}
}

}

/* Do properties. */
#define PROPERTY_CLASS PannedSource
#define PROPERTY_LIST PANNED_SOURCE_PROPERTIES
#define PROPERTY_BASE Source
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createPannedSource(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto ret = ctx->createObject<PannedSource>();
	std::shared_ptr<Source> src_ptr = ret;
	ctx->registerSource(src_ptr);
	*out = toC(ret);
	return 0;
	SYZ_EPILOGUE
}
