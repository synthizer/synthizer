#pragma once

#include "synthizer/block_delay_line.hpp"
#include "synthizer/config.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/effects/global_effect.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/types.hpp"

#include "concurrentqueue.h"

namespace synthizer {

struct EchoTapConfig {
	float gain_l = 0.5, gain_r = 0.5;
	unsigned int delay = 0;
};

/* For later. This will probably eventually hold, i.e., crossfading state. */
struct EchoTap {
 EchoTapConfig config;
};

/*
 * A stereo echo effect with variable taps. This gets to use the "echo" name because it is broadly speaking the most
 * common form of echo.
 * 
 * Right now the delay line length is hardcoded to approximately 5 seconds: this is because we've yet to need a dynamically sized delay line, so there's not currently
 * one implemented. I'm going to circle back on this later, but for the sake of early access, let's get this out the door without it.
 * */
class EchoEffect: public BaseEffect {
	public:
	void runEffect(unsigned int time_in_blocks, unsigned int input_channels, float *input, unsigned int output_channels, float *output, float gain) override;
	void resetEffect() override;

	void pushNewConfig(deferred_vector<EchoTapConfig> &&config);

	static const unsigned int MAX_DELAY = nextMultipleOf(config::SR * 5, config::BLOCK_SIZE);
	private:
	/*
	 * There's actually 4 entirely distinct scenarios, both of the following in any combination:
	 * 1. We just got new config (FADE_IN=true); and
	 * 2. We need to detour through an intermediate buffer to remix.
	 * 
	 * 1 changes at most once per config update and 2 is fixed for the lifetime of the context, so get the compiler to help us out:
	 * generate 4 versions, and rely on the fact that we never change which version we use to make up for code cache unfriendliness.
	 * 
	 * Assumes that the audio is already in the delay line, and that the parent will handle channel mixing on the way out.
	 * */
	template<bool FADE_IN, bool ADD>
	void runEffectInternal(float *output, float gain);

	BlockDelayLine<2, MAX_DELAY / config::BLOCK_SIZE> line;
	/*
	 * Configurations are pushed onto this queue, then picked up the next time runEffect is called.
	 * 
	 * This will probably be replaced with a box (see boxes.hpp) once we have a suitable one. That's on the to-do list.
	 * */
	moodycamel::ConcurrentQueue<deferred_vector<EchoTapConfig>> pending_configs;
	deferred_vector<EchoTap> taps;
	/* What we need to pass to runRead Loop. */
	unsigned int max_delay_tap = 0;
};

class GlobalEchoEffect: public GlobalEffect<EchoEffect> {
	public:
	template<typename... ARGS>
	GlobalEchoEffect(ARGS&&... args): GlobalEffect<EchoEffect>(std::forward<ARGS>(args)...) {}

	#include "synthizer/property_methods.hpp"
};

}
