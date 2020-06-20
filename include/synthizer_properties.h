#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * X macros for synthizer properties.
 * 
 * The format is:
 * 
 * #define OBJECT_TYPE_PROPERTIES \
 * pattern(args)
 * 
 * Where pattern can be:
 * int_p(SYZ_MY_PROPERTY, name, Name, min, max)
 * DOUBLE_p(SYZ_MY_PROPERTY, name, Name, min, max)
 * object_p(SYZ_MY_PROPERTY, name, Name, ExpectedClass)
 * double3_p(SYZ_MY_PROPERTY, name, Name)
 * double6_p(SYZ_MY_PROPERTY, name, Name);
 * 
 * For int properties, use P_INT_MIN and P_INT_MAX for no range; for double use P_DOUBLE_MIN and P_DOUBLE_MAX. Using these as only one of the endpoints is fine.
 * 
 * For internal usage, synthizer needs two versions of the name that aren't the constant. The first is of the form my_property and used for logging, etc. The second is of the form MyProperty (with leading capital) to link up with methods on C++ classes.
 * This header is public because it may be useful to use these in your own code, but you don't need it
 * if all you want to do is use the library.
 * */

#define CONTEXT_PROPERTIES \
DOUBLE3_P(SYZ_CONTEXT_LISTENER_POSITION, listener_position, ListenerPosition) \
DOUBLE6_P(SYZ_CONTEXT_LISTENER_ORIENTATION, listener_orientation, ListenerOrientation) \
INT_P(SYZ_CONTEXT_DISTANCE_MODEL, distance_model, DistanceModel, 0, SYZ_DISTANCE_MODEL_COUNT - 1) \
DOUBLE_P(SYZ_CONTEXT_DISTANCE_REF, distance_ref, DistanceRef,  0.0, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_CONTEXT_DISTANCE_MAX, distance_max, DistanceMax, 0.0, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_CONTEXT_ROLLOFF, rolloff, Rolloff, 0.0, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_CONTEXT_CLOSENESS_BOOST, closeness_boost, ClosenessBoost, P_DOUBLE_MIN, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_CONTEXT_CLOSENESS_BOOST_DISTANCE, closeness_boost_distance, ClosenessBoostDistance, 0.0, P_DOUBLE_MAX)

#define PANNED_SOURCE_PROPERTIES_COMMON \
DOUBLE_P(SYZ_PANNED_SOURCE_GAIN, gain, Gain, P_DOUBLE_MIN, P_DOUBLE_MAX) \
INT_P(SYZ_PANNED_SOURCE_PANNER_STRATEGY, panner_strategy, PannerStrategy, 0, SYZ_PANNER_STRATEGY_COUNT - 1) \

#define PANNED_SOURCE_PROPERTIES \
PANNED_SOURCE_PROPERTIES_COMMON \
DOUBLE_P(SYZ_PANNED_SOURCE_AZIMUTH, azimuth, Azimuth, 0.0f, 360.0f) \
DOUBLE_P(SYZ_PANNED_SOURCE_ELEVATION, elevation, Elevation, -90.0f, 90.0f)\
DOUBLE_P(SYZ_PANNED_SOURCE_PANNING_SCALAR, panning_scalar, PanningScalar, 0.0, 1.0)

#define SOURCE3D_PROPERTIES \
DOUBLE3_P(SYZ_SOURCE3D_POSITION, position, Position) \
DOUBLE6_P(SYZ_SOURCE3D_ORIENTATION, orientation, Orientation) \
INT_P(SYZ_SOURCE3D_DISTANCE_MODEL, distance_model, DistanceModel, 0, SYZ_DISTANCE_MODEL_COUNT - 1) \
DOUBLE_P(SYZ_SOURCE3D_DISTANCE_REF, distance_ref, DistanceRef, 0.0, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_SOURCE3D_DISTANCE_MAX, distance_max, DistanceMax, 0.0, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_SOURCE3D_ROLLOFF, rolloff, Rolloff, 0.0, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_SOURCE3D_CLOSENESS_BOOST, closeness_boost, ClosenessBoost, P_DOUBLE_MIN, P_DOUBLE_MAX) \
DOUBLE_P(SYZ_SOURCE3D_CLOSENESS_BOOST_DISTANCE, closeness_boost_distance, ClosenessBoostDistance, 0.0, P_DOUBLE_MAX)

#define BUFFER_GENERATOR_PROPERTIES \
OBJECT_P(SYZ_BUFFER_GENERATOR_BUFFER, buffer, Buffer, Buffer) \
DOUBLE_P(SYZ_BUFFER_GENERATOR_POSITION, position, Position, 0.0, P_DOUBLE_MAX)

#ifdef __cplusplus
}
#endif
