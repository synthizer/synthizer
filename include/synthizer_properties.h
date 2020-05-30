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
 * 
 * For int properties, use P_INT_MIN and P_INT_MAX for no range; for double use P_DOUBLE_MIN and P_DOUBLE_MAX. Using these as only one of the endpoints is fine.
 * 
 * For internal usage, synthizer needs two versions of the name that aren't the constant. The first is of the form my_property and used for logging, etc. The second is of the form MyProperty (with leading capital) to link up with methods on C++ classes.
 * This header is public because it may be useful to use these in your own code, but you don't need it
 * if all you want to do is use the library.
 * */


#define PANNED_SOURCE_PROPERTIES \
DOUBLE_P(SYZ_PANNED_SOURCE_AZIMUTH, azimuth, Azimuth, 0.0f, 360.0f) \
DOUBLE_P(SYZ_PANNED_SOURCE_ELEVATION, elevation, Elevation, -90.0f, 90.0f)\
DOUBLE_P(SYZ_PANNED_SOURCE_PANNING_SCALAR, panning_scalar, PanningScalar, 0.0, 1.0) \
DOUBLE_P(SYZ_PANNED_SOURCE_GAIN, gain, Gain, P_DOUBLE_MIN, P_DOUBLE_MAX) \
INT_P(SYZ_PANNED_SOURCE_PANNER_STRATEGY, panner_strategy, PannerStrategy, 0, SYZ_PANNER_STRATEGY_COUNT - 1) \

#ifdef __cplusplus
}
#endif
