#pragma once

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/memory.hpp"
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
	/* The gains, one per line. */
	std::array<float, LINES> gains = { { 0.5f } };
	std::array<unsigned int, LINES> delays = { { 0 } };

	/*
	 * When a property gets set, set to true to mark that we need to recompute the model.
	 * */
	bool recompute_model = true;
	/*
	 * The t60 of the reverb, in seconds.
	 * */
	float t60 = 100.0f;
	/*
	 * The mean free path, in seconds.
	 * This is effectively meters/ speed of sound.
	 * 0.01 is approximately 5 meters.
	 * */
	float mean_free_path = 0.05f;
	/*
	 * Diffusion is a measure of how fast reflections spread. This can't be equated to a physical property, so we just treat it as a
	 * percent. Internally, this feeds the algorithm which picks delay line lengths.
	 * */
	float late_reflections_diffusion = 0.5f;
};

class GlobalFdnReverbEffect: public GlobalEffect<FdnReverbEffect> {
	public:
	template<typename... ARGS>
	GlobalFdnReverbEffect(ARGS&&... args): GlobalEffect<FdnReverbEffect>(std::forward<ARGS>(args)...) {}

	PROPERTY_METHODS;
};

}
