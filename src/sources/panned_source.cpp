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

void PannedSource::setGain3D(double gain) {
	this->gain_3d = gain;
}

void PannedSource::run() {
	int panner_strategy;
	double azimuth, elevation, panning_scalar;

	bool invalid_lane = this->acquirePannerStrategy(panner_strategy);
	bool angles_changed = this->acquireAzimuth(azimuth) | this->acquireElevation(elevation);
	bool scalar_changed = this->acquirePanningScalar(panning_scalar);

	if (invalid_lane) {
		this->panner_lane = this->context->allocateSourcePannerLane((enum SYZ_PANNER_STRATEGY)panner_strategy);
	}

	if (angles_changed) {
		this->panner_lane->setPanningAngles(azimuth, elevation);
	} else if (scalar_changed) {
		this->panner_lane->setPanningScalar(panning_scalar);
	}

	this->fillBlock(1);
	/* And output. */
	this->panner_lane->update();
	unsigned int stride = this->panner_lane->stride;
	float *dest = this->panner_lane->destination;
	float g = this->gain_3d;
	for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
		dest[i * stride] = g * block[i];
	}
}

}

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
