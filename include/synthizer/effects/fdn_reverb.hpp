#pragma once

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/interpolated_random_sequence.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/three_band_eq.hpp"
#include "synthizer/types.hpp"

#include <utility>

namespace synthizer {

/*
 * An FDN reverberator using a household reflection matrix and an early reflections model.
 * */
class FdnReverbEffect: public BaseEffect {
	public:
	/* Number of lines in the FDN. */
	static constexpr unsigned int LINES = 8;
	/*
	 * Must be large enough to account for the maximum theoretical delay we might see. Figure that out buy summing the following properties's maximums:
	 * mean_free_path + late_reflections_modulation_depth.
	 * late_reflections_delay doesn't need to be accounted for, as long as its maximum is below the above sum.
	 * 
	 * Also, add in a little bit extra so that prime numbers above the range have room, since reading the primes array can go out of range.
	 * */
	static constexpr unsigned int MAX_DELAY_SAMPLES = config::SR * 1;
	static constexpr float MAX_DELAY_SECONDS = 1.0f;
	/*
	 * In the primes-finding algorithm, if we push up against the far end of what we can reasonably handle, we need to cap it.
	 * 
	 * The result is that extreme values will become periodic, but values this extreme aren't going to work anyway and it's better than crashing.
	 * 
	 * This must not be larger than mean free path + slack used to setmAX_DELAY_{SAMPLES,SECONDS}.
	 * */
	static constexpr unsigned int MAX_FEEDBACK_DELAY = 0.35 * config::SR;

	void runEffect(unsigned int time_in_blocks, unsigned int input_channels, float *input, unsigned int output_channels, float *output, float gain) override;
	void resetEffect() override;

	#define EXPOSE(TYPE, FIELD, GETTER, SETTER) \
	TYPE GETTER() { return this->FIELD; } \
	void SETTER(TYPE v) { this->FIELD = v; this->dirty = true; }


	EXPOSE(int, input_filter_enabled, getInputFilterEnabled, setInputFilterEnabled)
	EXPOSE(double, input_filter_cutoff, getInputFilterCutoff, setInputFilterCutoff)
	EXPOSE(double, mean_free_path, getMeanFreePath, setMeanFreePath)
	EXPOSE(double, t60, getT60, setT60)
	EXPOSE(double, late_reflections_lf_rolloff, getLateReflectionsLfRolloff, setLateReflectionsLfRolloff)
	EXPOSE(double, late_reflections_lf_reference, getLateReflectionsLfReference, setLateReflectionsLfReference)
	EXPOSE(double, late_reflections_hf_rolloff, getLateReflectionsHfRolloff, setLateReflectionsHfRolloff)
	EXPOSE(double, late_reflections_hf_reference, getLateReflectionsHfReference, setLateReflectionsHfReference)
	EXPOSE(double, late_reflections_diffusion, getLateReflectionsDiffusion, setLateReflectionsDiffusion)
	EXPOSE(double, late_reflections_modulation_depth, getLateReflectionsModulationDepth, setLateReflectionsModulationDepth)
	EXPOSE(double, late_reflections_modulation_frequency, getLateReflectionsModulationFrequency, setLateReflectionsModulationFrequency)
	EXPOSE(double, late_reflections_delay, getLateReflectionsDelay, setLateReflectionsDelay)

	protected:
	void recomputeModel();

	/*
	 * The delay, in samples, of the late reflections relative to the input.
	 * */
	unsigned int late_reflections_delay_samples;

	/*
	 * Used to filter the input.
	 * */
	IIRFilter<1, 3, 3> input_filter;
	/*
	 * The delay lines. We pack them into one to prevent needing multiple modulus.
	 * */
	BlockDelayLine<LINES, nextMultipleOf(MAX_DELAY_SAMPLES, config::BLOCK_SIZE) / config::BLOCK_SIZE> lines;
	std::array<unsigned int, LINES> delays = { { 0 } };
	/*
	 * Also accounts for the modulation depth.
	 * */
	unsigned int max_delay = 0;
	/*
	 * Allows control of frequency bands on the feedback paths, so that we can have different t60s.
	 * */
	ThreeBandEq<LINES> feedback_eq;
	/*
	 * Random number generator for modulating the delay lines in the late reflection.
	 * */
	std::array<InterpolatedRandomSequence, LINES> late_modulators;

	/*
	 * When a property gets set, set to true to mark that we need to recompute the model.
	 * */
	bool dirty = true;

	/*
	 * The input filter is a lowpass for now. We might let this be configurable later.
	 * */
	bool input_filter_enabled = true;
	float input_filter_cutoff = 2000.0f;

	/*
	 * The mean free path, in seconds.
	 * This is effectively meters/ speed of sound.
	 * 0.01 is approximately 5 meters.
	 * */
	float mean_free_path = 0.02f;
	/*
	 * The t60 of the reverb, in seconds.
	 * */
	float t60 = 1.0f;
	/*
	 * rolloff ratios. The effective t60 for a band of the equalizer is rolloff * t60.
	 * */
	float late_reflections_lf_rolloff = 1.0f;
	float late_reflections_hf_rolloff = 0.5f;
	/*
	 * Defines the bands of the eq filter for late reflections.
	 * */
	float late_reflections_lf_reference = 200.0f;
	float late_reflections_hf_reference = 500.0f;
	/*
	 * Diffusion is a measure of how fast reflections spread. This can't be equated to a physical property, so we just treat it as a
	 * percent. Internally, this feeds the algorithm which picks delay line lengths.
	 * */
	float late_reflections_diffusion = 1.0f;
	/*
	 * How much modulation in the delay lines? Larger values reduce periodicity, at the cost of introducing chorus-like effects.
	 * In seconds.
	 * */
	float late_reflections_modulation_depth = 0.01f;
	/*
	 * Frequency of modulation changes in hz.
	 * */
	float late_reflections_modulation_frequency = 0.5f;
	float late_reflections_delay = 0.01f;
};

class GlobalFdnReverbEffect: public GlobalEffect<FdnReverbEffect> {
	public:
	template<typename... ARGS>
	GlobalFdnReverbEffect(ARGS&&... args): GlobalEffect<FdnReverbEffect>(std::forward<ARGS>(args)...) {}

	PROPERTY_METHODS;
};

}
