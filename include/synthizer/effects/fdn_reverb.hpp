#pragma once

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/effects/global_effect.hpp"
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
	/* Maximum number of lines in the FDN. */
	static constexpr unsigned int LINES = 8;
	/* Must match data_processor/main.py's maximum prime. */
	static constexpr unsigned int MAX_DELAY_SAMPLES = config::SR * 5;
	static constexpr float MAX_DELAY_SECONDS = 5.0f;

	void runEffect(unsigned int time_in_blocks, unsigned int input_channels, AudioSample *input, unsigned int output_channels, AudioSample *output) override;
	void resetEffect() override;

	double getLateReflectionsDiffusion();
	void setLateReflectionsDiffusion(double value);
	double getT60();
	void setT60(double value);
	double getMeanFreePath();
	void setMeanFreePath(double value);

	protected:
	void recomputeModel();

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
	bool recompute_model = true;
	/*
	 * The t60 of the reverb, in seconds.
	 * */
	float t60 = 2.5f;
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
	 * The mean free path, in seconds.
	 * This is effectively meters/ speed of sound.
	 * 0.01 is approximately 5 meters.
	 * */
	float mean_free_path = 0.04f;
	/*
	 * Diffusion is a measure of how fast reflections spread. This can't be equated to a physical property, so we just treat it as a
	 * percent. Internally, this feeds the algorithm which picks delay line lengths.
	 * */
	float late_reflections_diffusion = 1.0f;
	/*
	 * How much modulation in the delay lines? Larger values reduce periodicity, at the cost of introducing chorus-like effects.
	 * In seconds.
	 * */
	float late_reflections_modulation_depth = 0.001f;
	/*
	 * Frequency of modulation changes in hz.
	 * */
	float late_reflections_modulation_frequency = 0.5f;
};

class GlobalFdnReverbEffect: public GlobalEffect<FdnReverbEffect> {
	public:
	template<typename... ARGS>
	GlobalFdnReverbEffect(ARGS&&... args): GlobalEffect<FdnReverbEffect>(std::forward<ARGS>(args)...) {}

	PROPERTY_METHODS;
};

}
