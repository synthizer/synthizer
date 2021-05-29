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
 * int_p(SYZ_MY_PROPERTY, name, Name, min, max, default)
 * DOUBLE_p(SYZ_MY_PROPERTY, name, Name, min, max, default)
 * object_p(SYZ_MY_PROPERTY, name, Name, ExpectedClass)
 * double3_p(SYZ_MY_PROPERTY, name, Name, default_x, default_y, default_z)
 * double6_p(SYZ_MY_PROPERTY, name, Name, default_x, default_y, default_z, default_x2, default_y2, default_z2)
 * BIQUAD_P(SYZ_P_MY_PROPERTY, name, Name)
 * 
 * For int properties, use P_INT_MIN and P_INT_MAX for no range; for double use P_DOUBLE_MIN and P_DOUBLE_MAX. Using these as only one of the endpoints is fine.
 * 
 * Synthizer needs two versions of the name that aren't the constant. The first is of the form my_property and used for logging, etc. The second is of the form MyProperty (with leading capital) to link up with methods on C++ classes.
 * */

#define DISTANCE_MODEL_PROPERTIES(A, B, C) \
INT_P(SYZ_P##A##DISTANCE_MODEL, B##distance_model, C##DistanceModel, 0, SYZ_DISTANCE_MODEL_COUNT - 1, SYZ_DISTANCE_MODEL_LINEAR) \
DOUBLE_P(SYZ_P##A##DISTANCE_REF, B##distance_ref, C##DistanceRef, 0.0, P_DOUBLE_MAX, 1.0) \
DOUBLE_P(SYZ_P##A##DISTANCE_MAX, B##distance_max, C##DistanceMax, 0.0, P_DOUBLE_MAX, 50.0) \
DOUBLE_P(SYZ_P##A##ROLLOFF, B##rolloff, C##Rolloff, 0.0, P_DOUBLE_MAX, 1.0) \
DOUBLE_P(SYZ_P##A##CLOSENESS_BOOST, B##closeness_boost, C##ClosenessBoost, P_DOUBLE_MIN, P_DOUBLE_MAX, 0.0) \
DOUBLE_P(SYZ_P##A##CLOSENESS_BOOST_DISTANCE, B##closeness_boost_distance, C##ClosenessBoostDistance, 0.0, P_DOUBLE_MAX, 0.0)

#define STANDARD_DISTANCE_MODEL_PROPERTIES DISTANCE_MODEL_PROPERTIES(_, ,)

#define CONTEXT_PROPERTIES \
DOUBLE_P(SYZ_P_GAIN, gain, Gain, 0.0, P_DOUBLE_MAX, 1.0) \
DOUBLE3_P(SYZ_P_POSITION, position, Position, 0.0, 0.0, 0.0) \
DOUBLE6_P(SYZ_P_ORIENTATION, orientation, Orientation, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0) \
INT_P(SYZ_P_DEFAULT_PANNER_STRATEGY, default_panner_strategy, DefaultPannerStrategy, 0, SYZ_PANNER_STRATEGY_COUNT - 1, SYZ_PANNER_STRATEGY_STEREO) \
DISTANCE_MODEL_PROPERTIES(_DEFAULT_, default_, Default)

#define SOURCE_PROPERTIES \
DOUBLE_P(SYZ_P_GAIN, gain, Gain, 0.0, P_DOUBLE_MAX, 1.0) \
BIQUAD_P(SYZ_P_FILTER, filter, Filter) \
BIQUAD_P(SYZ_P_FILTER_DIRECT, filter_direct, FilterDirect) \
BIQUAD_P(SYZ_P_FILTER_EFFECTS, filter_effects, FilterEffects) \


#define PANNED_SOURCE_PROPERTIES \
INT_P(SYZ_P_PANNER_STRATEGY, panner_strategy, PannerStrategy, 0, SYZ_PANNER_STRATEGY_COUNT - 1, SYZ_PANNER_STRATEGY_STEREO) \
DOUBLE_P(SYZ_P_AZIMUTH, azimuth, Azimuth, 0.0f, 360.0f, 0.0) \
DOUBLE_P(SYZ_P_ELEVATION, elevation, Elevation, -90.0f, 90.0f, 0.0)\
DOUBLE_P(SYZ_P_PANNING_SCALAR, panning_scalar, PanningScalar, -1.0, 1.0, 0.0)

#define SOURCE3D_PROPERTIES \
DOUBLE3_P(SYZ_P_POSITION, position, Position, 0.0, 0.0, 0.0) \
DOUBLE6_P(SYZ_P_ORIENTATION, orientation, Orientation, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0) \
STANDARD_DISTANCE_MODEL_PROPERTIES

#define GENERATOR_PROPERTIES \
DOUBLE_P(SYZ_P_PITCH_BEND, pitch_bend, PitchBend, 0.0, P_DOUBLE_MAX, 1.0) \
DOUBLE_P(SYZ_P_GAIN, gain, Gain, 0.0, P_DOUBLE_MAX, 1.0)

#define BUFFER_GENERATOR_PROPERTIES \
OBJECT_P(SYZ_P_BUFFER, buffer, Buffer, Buffer) \
DOUBLE_P(SYZ_P_PLAYBACK_POSITION, playback_position, PlaybackPosition, 0.0, P_DOUBLE_MAX, 0.0) \
INT_P(SYZ_P_LOOPING, looping, Looping, 0, 1, 0) \

#define STREAMING_GENERATOR_PROPERTIES \
DOUBLE_P(SYZ_P_PLAYBACK_POSITION, playback_position, PlaybackPosition, 0.0, P_DOUBLE_MAX, 0.0) \
INT_P(SYZ_P_LOOPING, looping, Looping, 0, 1, 0)

#define NOISE_GENERATOR_PROPERTIES \
INT_P(SYZ_P_NOISE_TYPE, noise_type, NoiseType, 0, SYZ_NOISE_TYPE_COUNT - 1, SYZ_NOISE_TYPE_UNIFORM)

#define EFFECT_PROPERTIES \
DOUBLE_P(SYZ_P_GAIN, gain, Gain, 0.0, P_DOUBLE_MAX, 1.0) \
BIQUAD_P(SYZ_P_FILTER_INPUT, filter_input, FilterInput)

#define FDN_REVERB_EFFECT_PROPERTIES \
DOUBLE_P(SYZ_P_MEAN_FREE_PATH, mean_free_path, MeanFreePath, 0.0, 0.5, 0.1) \
DOUBLE_P(SYZ_P_T60, t60, T60, 0.0, 100.0, 0.3) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_LF_ROLLOFF, late_reflections_lf_rolloff, LateReflectionsLfRolloff, 0.0, 2.0, 1.0) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_LF_REFERENCE, late_reflections_lf_reference, LateReflectionsLfReference, 0.0, 22050.0, 200.0) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_HF_ROLLOFF, late_reflections_hf_rolloff, LateReflectionsHfRolloff, 0.0, 2.0, 0.5) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_HF_REFERENCE, late_reflections_hf_reference, LateReflectionsHfReference, 0.0, 22050.0, 500.0) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_DIFFUSION, late_reflections_diffusion, LateReflectionsDiffusion, 0.0, 1.0, 1.0) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_MODULATION_DEPTH, late_reflections_modulation_depth, LateReflectionsModulationDepth, 0.0, 0.3, 0.01) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_MODULATION_FREQUENCY, late_reflections_modulation_frequency, LateReflectionsModulationFrequency, 0.01, 100.0, 0.5) \
DOUBLE_P(SYZ_P_LATE_REFLECTIONS_DELAY, late_reflections_delay, LateReflectionsDelay, 0.0, 0.5, 0.03)

#ifdef __cplusplus
}
#endif
