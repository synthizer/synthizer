#pragma once

#include "synthizer/context.hpp"
#include "synthizer/effects/base_effect.hpp"
#include "synthizer/routable.hpp"
#include "synthizer/types.hpp"

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
 * A global effect is an effect which soruces route to on a one-by-one basis. To use this, apply the CRTP.
 * */
template<typename BASE>
class GlobalEffect: public BASE, public RouteInput, public GlobalEffectBase {
	/*
	 * This constructor expects the fixed arguments for us,
	 * then forwards the parameter pack to BASE. This lets us pass arguments through without having to tie effect implementations
	 * to the object infrastructure (i.e. we can reuse them internally if we want).
	 * */
	template<typename... ARGS>
	GlobalEffect(std::shared_ptr<Context> &ctx, unsigned int channels, ARGS&& ...args):
		BASE(std::forward(args)...),
		RouteInput(ctx, channels, &this->input_buffer[0]),
		channels(channels)
		 {}

	void run(unsigned int channels, AudioSample *destination) {
		this->runEffect(this->time_in_blocks, this->channels, &this->input_buffer[0], channels, destination);
		this->time_in_blocks++;
	}

	private:
	alignas(config::ALIGNMENT) std::array<AudioSample, config::BLOCK_SIZE * config::MAX_CHANNELS> input_buffer = { 0.0f };
	unsigned int channels = 0;
	unsigned int time_in_blocks = 0;
};

}
