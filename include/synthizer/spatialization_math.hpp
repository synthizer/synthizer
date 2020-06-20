#pragma once

#include "synthizer_constants.h"

#include <array>
#include <cmath>

namespace synthizer {

struct DistanceParams {
	double distance;
	double distance_ref = 1.0;
	double distance_max = 50.0;
	double rolloff = 1.0;
	double closeness_boost = 0.0;
	double closeness_boost_distance = 0.0;
	enum SYZ_DISTANCE_MODEL distance_model = SYZ_DISTANCE_MODEL_LINEAR;
};

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