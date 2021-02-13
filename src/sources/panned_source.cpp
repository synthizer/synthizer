#include "synthizer.h"
#include "synthizer_constants.h"

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
	Source::initInAudioThread();
	/* Copy the default in from the context. */
	this->setPannerStrategy(this->getContextRaw()->getPannerStrategy());
	/**
	 * This is a hack to fix panning_scalar if set in the same tick as creation of the source. See issue #52.
	 * 
	 * The underlying problem is that all properties start off marked as changed. This greatly simplifies the case in which objects
	 * have entirely orthogonal properties by avoiding the need for an explicit initialization step.  Unfortunately, PannedSource controls whether or not
	 * to use azimuth/elevation or panning_scalar based on which was set last.  This is in part because such an arrrangement is convenient for the user
	 * and in part because the old property infrastructure called explicit methods on the class.  Because explicit methods could be used to flip some booleans, this used to not be a problem.
	 * 
	 * Fortunately, there is a total order on all operations on objects, and users can't set properties until after they've fully initialized.  We can abuse this: by clearing the 3 specific properties here and relying
	 * on the fact that everything in panning already defaults to match the defaults of PannedSource as it is, we can then detect any writes that happen in the first tick.
	 * 
	 * If we continue having issues like this, Synthizer will need to adopt a more wholistic solution, but this is good enough for now.
	 * */
	double v;
	acquireAzimuth(v);
	acquireElevation(v);
	acquirePanningScalar(v);
}

int PannedSource::getObjectType() {
	return SYZ_OTYPE_PANNED_SOURCE;
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

	if (angles_changed || invalid_lane) {
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
