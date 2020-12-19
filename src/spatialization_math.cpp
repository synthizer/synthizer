#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/error.hpp"
#include "synthizer/math.hpp"
#include "synthizer/spatialization_math.hpp"

#include <algorithm>
#include <cmath>

namespace synthizer {

double mulFromDistanceParams(const DistanceParams &params) {
	double dist_mul = 1.0;

	switch (params.distance_model) {
	case SYZ_DISTANCE_MODEL_NONE:
		dist_mul = 1.0;
		break;
	case SYZ_DISTANCE_MODEL_LINEAR:
		dist_mul = 1 - params.rolloff * (clamp(params.distance, params.distance_ref, params.distance_max) - params.distance_ref) / (params.distance_max - params.distance_ref);
		break;
	case SYZ_DISTANCE_MODEL_EXPONENTIAL:
		if (params.distance_ref == 0.0) {
			dist_mul = 0.0;
		} else {
			dist_mul = std::pow(std::max(params.distance, params.distance_ref) / params.distance_ref, -params.rolloff);
		}
		break;
	case SYZ_DISTANCE_MODEL_INVERSE:
		if (params.distance_ref == 0.0) {
			dist_mul = 0.0;
		} else {
			dist_mul = params.distance_ref / (params.distance_ref + params.rolloff * std::max(params.distance, params.distance_ref) - params.distance_ref);
		}
		break;
	default:
		dist_mul = 1.0;
	}

	/*
	 * If the distance is further away than closeness_boost, we reduce.
	 * 
	 * This is counterintuitive, but makes sure that distances  closer than closeness_boost don't break attenuation.
	 * */
	if (params.distance > params.closeness_boost_distance) {
		dist_mul *= dbToGain(-params.closeness_boost);
	}

	return clamp(dist_mul, 0.0, 1.0);
}

void throwIfParallel(const Vec3d &a, const Vec3d &b) {
	double dp = dotProduct(a, b);
	if (dp > 0.95) throw EInvariant("Vectors must not be parallel");
}

DistanceParams &DistanceParamsMixin::getDistanceParams() {
	return this->distance_params;
}

void DistanceParamsMixin::setDistanceParams(const DistanceParams &params) {
	this->distance_params = params;
}

double DistanceParamsMixin::getDistanceRef() {
	return this->distance_params.distance_ref;
}

void DistanceParamsMixin::setDistanceRef(double val) {
	this->distance_params.distance_ref = val;
}

double DistanceParamsMixin::getDistanceMax() {
	return this->distance_params.distance_max;
}

void DistanceParamsMixin::setDistanceMax(double val) {
	this->distance_params.distance_max = val;
}

double DistanceParamsMixin::getRolloff() {
	return this->distance_params.rolloff;
}

void DistanceParamsMixin::setRolloff(double val) {
	this->distance_params.rolloff = val;
}
double DistanceParamsMixin::getClosenessBoost() {
	return this->distance_params.closeness_boost;
}

void DistanceParamsMixin::setClosenessBoost(double val) {
	this->distance_params.closeness_boost = val;
}

double DistanceParamsMixin::getClosenessBoostDistance() {
	return this->distance_params.closeness_boost_distance;
}

void DistanceParamsMixin::setClosenessBoostDistance(double val) {
	this->distance_params.closeness_boost_distance = val;
}

int DistanceParamsMixin::getDistanceModel() {
	return this->distance_params.distance_model;
}

void DistanceParamsMixin::setDistanceModel(int val) {
	this->distance_params.distance_model = (enum SYZ_DISTANCE_MODEL)val;
}

}
