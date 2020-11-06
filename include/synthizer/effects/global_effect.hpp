#pragma once

#include "synthizer/context.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/routable.hpp"
#include "synthizer/types.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

namespace synthizer {

/*
 * helper shim to let us register with Context while applying the CRTP.
 * */
class GlobalEffectBase {
	public:
	virtual void run(unsigned int channels, AudioSample *output) = 0;
};

/*
 * A global effect is an effect which sources route to on a one-by-one basis. To use this, apply the CRTP.
 * */
template<typename BASE>
class GlobalEffect: public BASE, public RouteInput, public GlobalEffectBase {
	public:
	/*
	 * This constructor expects the fixed arguments for us,
	 * then forwards the parameter pack to BASE. This lets us pass arguments through without having to tie effect implementations
	 * to the object infrastructure (i.e. we can reuse them internally if we want).
	 * */
	#pragma clang diagnostic push
	/* We need to suppress this because RouteInput needs the address of the buffer, which isn't initialized until the derive class finishes. */
	#pragma clang diagnostic ignored "-Wuninitialized"
	template<typename... ARGS>
	GlobalEffect(const std::shared_ptr<Context> &ctx, unsigned int channels, ARGS&& ...args):
		BASE(std::forward(args)...),
		RouteInput(ctx, &this->input_buffer[0], channels),
		channels(channels)
		 {}
	#pragma clang diagnostic pop

	void run(unsigned int channels, AudioSample *destination) {
		this->runEffect(this->time_in_blocks, this->channels, &this->input_buffer[0], channels, destination, 1.0f);
		/*
		 * Reset this for the next time. This needs to live here since routers don't know about effects if
		 * there's no routing to them.
		 * */
		std::fill(&this->input_buffer[0], &this->input_buffer[this->channels * config::BLOCK_SIZE], 0.0f);
		this->time_in_blocks++;
	}

	private:
	alignas(config::ALIGNMENT) std::array<AudioSample, config::BLOCK_SIZE * config::MAX_CHANNELS> input_buffer = { 0.0f };
	unsigned int channels = 0;
	unsigned int time_in_blocks = 0;
};

}
