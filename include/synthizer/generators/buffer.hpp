#pragma once

#include "synthizer/buffer.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/types.hpp"

#include <memory>

namespace synthizer {

class BufferGenerator: public Generator {
	public:
	BufferGenerator(std::shared_ptr<Context> ctx): Generator(ctx) {}

	unsigned int getChannels() override;
	void generateBlock(float *output) override;

	#define PROPERTY_CLASS BufferGenerator
	#define PROPERTY_BASE Generator
	#define PROPERTY_LIST BUFFER_GENERATOR_PROPERTIES
	#include "synthizer/property_impl_new.hpp"

	private:
	template<bool L>
	void readInterpolated(double pos, float *out);
	/* Adds to destination, per the generators API. */
	void generateNoPitchBend(float *out);
	template<bool L>
	void generatePitchBendHelper(float *out, double pitch_bend);
	void generatePitchBend(float *out, double pitch_bend);
	void configureBufferReader(const std::shared_ptr<Buffer> &b);

	BufferReader reader;
};

}
