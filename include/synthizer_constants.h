#pragma once

#ifdef __cplusplus
extern "C" {
#endif 

enum SYZ_PANNER_STRATEGIES {
	SYZ_PANNER_STRATEGY_HRTF = 0,
	SYZ_PANNER_STRATEGY_STEREO = 1,
	SYZ_PANNER_STRATEGY_COUNT,
};

/*
 * Properties. by object type.
 * 
 * We leave room between these enums so that these can be extended; synthizer links these in an object oriented hierarchy.
 * 
 * In general greater properties are on more-derived objects, and the leading digits try to specify how derived.
 * */

enum SYZ_PANNED_SOURCE_PROPERTIES {
	SYZ_PANNED_SOURCE_AZIMUTH = 200,
	SYZ_PANNED_SOURCE_ELEVATION = 201,
	SYZ_PANNED_SOURCE_PANNING_SCALAR = 202,
	SYZ_PANNED_SOURCE_PANNER_STRATEGY = 203,
	SYZ_PANNED_SOURCE_GAIN = 204,
};

#ifdef __cplusplus
}
#endif
