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
 * A global effect is an effect which sources route to on a one-by-one basis. To use this, apply the CRTP.
 * */
class GlobalEffect: public BaseEffect, public RouteInput {
	public:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
	GlobalEffect(const std::shared_ptr<Context> &ctx, unsigned int channels):
		input_buffer(),
		channels(channels),
		RouteInput(ctx, &this->input_buffer[0], channels) {}
#pragma clang diagnostic pop

 double getGain() {
	 return this->gain;
 }

	void setGain(double g) {
		this->gain = g;
	}

	void run(unsigned int channels, float *destination) {
		this->runEffect(this->time_in_blocks, this->channels, &this->input_buffer[0], channels, destination, this->gain);
		/*
		 * Reset this for the next time. This needs to live here since routers don't know about effects if
		 * there's no routing to them.
		 * */
		std::fill(&this->input_buffer[0], &this->input_buffer[this->channels * config::BLOCK_SIZE], 0.0f);
		this->time_in_blocks++;
	}

	#include "synthizer/property_methods.hpp"
	private:
	alignas(config::ALIGNMENT) std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> input_buffer = { 0.0f };
	unsigned int channels = 0;
	unsigned int time_in_blocks = 0;
	float gain = 1.0f;
};

}
