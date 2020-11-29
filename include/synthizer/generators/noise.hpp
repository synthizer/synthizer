#pragma once

#include "synthizer.h"

#include "synthizer/context.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/noise_generator.hpp"
#include "synthizer/property_internals.hpp"

#include <memory>

namespace synthizer {

/*
 * The odd naming is so that NoiseGenerator can be the implementation, so we can reuse it for
 * i.e. reverb.
 * */
class ExposedNoiseGenerator: public Generator {
	public:
	ExposedNoiseGenerator(const std::shared_ptr<Context> &ctx, int channels): Generator(ctx), channels(channels)  {
		for (unsigned int i = 0; i < channels; i++) {
			this->generators.emplace_back();
		}
	}

	unsigned int getChannels() override {
		return this->channels;
	}

	void generateBlock(float *out) override;
	void setPitchBend(double newPitchBend) override;

	int getNoiseType();
	void setNoiseType(int type);

	#include "synthizer/property_methods.hpp"
	private:
	deferred_vector<NoiseGenerator> generators;
	unsigned int channels;
};

}
