#pragma once

#include "synthizer_constants.h"

#include "synthizer/error.hpp"
#include "synthizer/math.hpp"

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

  /* records whether or not we think that this set of parameters has changed. Used when materializing from properties.
   */
  bool changed = false;
};

/*
 * These templates materialize and/or set properties on anything with all of the properties for distance models.
 *
 * They're not instance methods because the distance model struct itself will probably become public as part of being
 * able to configure effect sends.
 * */
template <typename T> DistanceParams materializeDistanceParamsFromProperties(T *source) {
  DistanceParams ret;
  int distance_model;

  // The cast silences a warning.
  ret.changed = (int)source->acquireDistanceRef(ret.distance_ref) | source->acquireDistanceMax(ret.distance_max) |
                source->acquireRolloff(ret.rolloff) | source->acquireClosenessBoost(ret.closeness_boost) |
                source->acquireClosenessBoostDistance(ret.closeness_boost_distance) |
                /* This one needs a cast, so pull it to a different vairable first. */
                source->acquireDistanceModel(distance_model);
  ret.distance_model = (enum SYZ_DISTANCE_MODEL)distance_model;
  return ret;
}

/**
 * Used on e.g. contexts, to build one from defaults.
 * */
template <typename T> DistanceParams materializeDistanceParamsFromDefaultProperties(T *source) {
  DistanceParams ret;
  int distance_model;

  // The cast silences a clang warning.
  ret.changed = (int)source->acquireDefaultDistanceRef(ret.distance_ref) |
                source->acquireDefaultDistanceMax(ret.distance_max) | source->acquireDefaultRolloff(ret.rolloff) |
                source->acquireDefaultClosenessBoost(ret.closeness_boost) |
                source->acquireDefaultClosenessBoostDistance(ret.closeness_boost_distance) |
                /* This one needs a cast, so pull it to a different vairable first. */
                source->acquireDefaultDistanceModel(distance_model);
  ret.distance_model = (enum SYZ_DISTANCE_MODEL)distance_model;
  return ret;
}

template <typename T> void setPropertiesFromDistanceParams(T *dest, const DistanceParams &&params) {
  dest->setDistanceRef(params.distance_ref);
  dest->setDistanceMax(params.distance_max);
  dest->setRolloff(params.rolloff);
  dest->setClosenessBoost(params.closeness_boost);
  dest->setClosenessBoostDistance(params.closeness_boost_distance);
  dest->setDistanceModel((int)params.distance_model);
}

inline double mulFromDistanceParams(const DistanceParams &params) {
  double dist_mul = 1.0;

  switch (params.distance_model) {
  case SYZ_DISTANCE_MODEL_NONE:
    dist_mul = 1.0;
    break;
  case SYZ_DISTANCE_MODEL_LINEAR:
    dist_mul = 1 - params.rolloff *
                       (clamp(params.distance, params.distance_ref, params.distance_max) - params.distance_ref) /
                       (params.distance_max - params.distance_ref);
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
      dist_mul =
          params.distance_ref /
          (params.distance_ref + params.rolloff * std::max(params.distance, params.distance_ref) - params.distance_ref);
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

typedef std::array<double, 3> Vec3d;

inline double dotProduct(const Vec3d &a, const Vec3d &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

inline Vec3d crossProduct(const Vec3d &a, const Vec3d &b) {
  return {
      a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0],
  };
}

inline double magnitude(const Vec3d &x) { return std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]); }

inline double distance(const Vec3d &a, const Vec3d &b) { return magnitude({a[0] - b[0], a[1] - b[1], a[2] - b[2]}); }

inline Vec3d normalize(const Vec3d &x) {
  double m = magnitude(x);
  return {x[0] / m, x[1] / m, x[2] / m};
}

inline void throwIfParallel(const Vec3d &a, const Vec3d &b) {
  double dp = dotProduct(a, b);
  if (dp > 0.95)
    throw EInvariant("Vectors must not be parallel");
}

} // namespace synthizer