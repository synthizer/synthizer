#pragma once

#include "synthizer_constants.h"

#include <array>
#include <cmath>

namespace synthizer {

struct DistanceParams {
	/* Distance is set by the user. */
	double distance = 0.0;

	double distance_ref = 1.0;
	double distance_max = 50.0;
	double rolloff = 1.0;
	double closeness_boost = 0.0;
	double closeness_boost_distance = 0.0;
	enum SYZ_DISTANCE_MODEL distance_model = SYZ_DISTANCE_MODEL_LINEAR;

	/* records whether or not we think that this set of parameters has changed. Used when materializing from properties. */
	bool changed = false;
};

/*
 * These templates materialize and/or set properties on anything with all of the properties for distance models.
 * 
 * They're not instance methods because the distance model struct itself will probably become public as part of being able to configure effect sends.
 * */
template<typename T>
DistanceParams materializeDistanceParamsFromProperties(T *source) {
	DistanceParams ret;
	int distance_model;

	ret.changed = source->acquireDistanceRef(ret.distance_ref) |
		source->acquireDistanceMax(ret.distance_max) |
		source->acquireRolloff(ret.rolloff) |
		source->acquireClosenessBoost(ret.closeness_boost) |
		source->acquireClosenessBoostDistance(ret.closeness_boost_distance) |
		/* This one needs a cast, so pull it to a different vairable first. */
		source->acquireDistanceModel(distance_model);
	ret.distance_model = (enum SYZ_DISTANCE_MODEL)distance_model;
	return ret;
}

/**
 * Used on e.g. contexts, to build one from defaults.
 * */
template<typename T>
DistanceParams materializeDistanceParamsFromDefaultProperties(T *source) {
	DistanceParams ret;
	int distance_model;

	ret.changed = source->acquireDefaultDistanceRef(ret.distance_ref) |
		source->acquireDefaultDistanceMax(ret.distance_max) |
		source->acquireDefaultRolloff(ret.rolloff) |
		source->acquireDefaultClosenessBoost(ret.closeness_boost) |
		source->acquireDefaultClosenessBoostDistance(ret.closeness_boost_distance) |
		/* This one needs a cast, so pull it to a different vairable first. */
		source->acquireDefaultDistanceModel(distance_model);
	ret.distance_model = (enum SYZ_DISTANCE_MODEL)distance_model;
	return ret;
}

template<typename T>
void setPropertiesFromDistanceParams(T *dest, const DistanceParams&& params) {
	dest->setDistanceRef(params.distance_ref);
	dest->setDistanceMax(params.distance_max);
	dest->setRolloff(params.rolloff);
	dest->setClosenessBoost(params.closeness_boost);
	dest->setClosenessBoostDistance(params.closeness_boost_distance);
	dest->setDistanceModel((int)params.distance_model);
}


double mulFromDistanceParams(const DistanceParams &params);

/*
 * Helper class which can be used to augment anything with a distance model, with Synthizer-compatible property getter/setter pairs.
 * 
 * How this works is you inherit from it somewhere in the hierarchy, then define properties as usual
 * */
class DistanceParamsMixin {
	public:
	DistanceParams &getDistanceParams();
	void setDistanceParams(const DistanceParams &params);

	double getDistanceRef();
	void setDistanceRef(double val);
	double getDistanceMax();
	void setDistanceMax(double val);
	double getRolloff();
	void setRolloff(double val);
	double getClosenessBoost();
	void setClosenessBoost(double val);
	double getClosenessBoostDistance();
	void setClosenessBoostDistance(double val);
	int getDistanceModel();
	void setDistanceModel(int val);

	private:
	DistanceParams distance_params{};
};

typedef std::array<double, 3> Vec3d;

inline double dotProduct(const Vec3d &a, const Vec3d &b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline Vec3d crossProduct(const Vec3d &a, const Vec3d &b) {
	return {
		a[1] * b[2] - a[2] * b[1],
		a[2] * b[0] - a[0] * b[2],
		a[0] * b[1] - a[1] * b[0],
	};
}

inline double magnitude(const Vec3d &x) {
	return std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
}

inline double distance(const Vec3d &a, const Vec3d &b) {
	return magnitude({ a[0] - b[0], a[1] - b[1], a[2] - b[2] });
}

inline Vec3d normalize(const Vec3d &x) {
	double m = magnitude(x);
	return { x[0] / m, x[1] / m, x[2] / m };
}

void throwIfParallel(const Vec3d &a, const Vec3d &b);

}