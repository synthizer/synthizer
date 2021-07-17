#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/math.hpp"
#include "synthizer/sources.hpp"
#include "synthizer/spatialization_math.hpp"

#include <cmath>

namespace synthizer {

Source3D::Source3D(std::shared_ptr<Context> context): PannedSource(context) {
}

void Source3D::initInAudioThread() {
	PannedSource::initInAudioThread();
	setPropertiesFromDistanceParams(this, this->getContext()->getDefaultDistanceParams());
}

int Source3D::getObjectType() {
	return SYZ_OTYPE_SOURCE_3D;
}

void Source3D::run() {
	auto ctx = this->getContext();
	auto listener_pos = ctx->getPosition();
	auto listener_orient = ctx->getOrientation();
	Vec3d listener_at = { listener_orient[0], listener_orient[1], listener_orient[2] };
	Vec3d listener_up = { listener_orient[3], listener_orient[4], listener_orient[5] };

	auto position_prop = this->getPosition();
	Vec3d pos = { position_prop[0] - listener_pos[0], position_prop[1] - listener_pos[1], position_prop[2] - listener_pos[2] };
	Vec3d at = normalize(listener_at);
	Vec3d right = normalize(crossProduct(listener_at, listener_up));
	Vec3d up = crossProduct(right, at);

	/* Use the above to go to a coordinate system where positive y is forward, positive x is right, positive z is up. */
	double y = dotProduct(at, pos);
	double x = dotProduct(right, pos);
	double z = dotProduct(up, pos);

	/* Now get spherical coordinates. */
	double dist = magnitude(pos);
	if (dist == 0) {
		/* Source is at the center of the head. Arbitrarily decide on forward. */
		x = 0;
		y = 1;
		z = 0;
	} else {
		x /= dist;
		y /= dist;
		z /= dist;
	}
	/* Remember: this is clockwise of north and atan2 is -pi to pi. */
	double azimuth = std::atan2(x, y);
	double elevation = std::atan2(z, std::sqrt(x * x + y * y));

	azimuth = std::fmod(azimuth * 180 / PI + 360, 360.0);
	this->setAzimuth(clamp(azimuth, 0.0, 360.0));
	elevation = clamp(elevation * 180 / PI, -90.0, 90.0);
	this->setElevation(elevation);
	auto dp = materializeDistanceParamsFromProperties(this);
	dp.distance = dist;
	this->setGain3D(mulFromDistanceParams(dp));
	assert(azimuth >= 0.0);
	PannedSource::run();
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createSource3D(syz_Handle *out, syz_Handle context, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto ret = ctx->createObject<Source3D>();
	std::shared_ptr<Source> src_ptr = ret;
	ctx->registerSource(src_ptr);
	*out = toC(ret);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}
