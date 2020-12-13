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
	std::shared_ptr<Buffer> getBuffer();
	void setBuffer(const std::shared_ptr<Buffer> &buffer);
	int getLooping();
	void setLooping(int l);
	double getPosition();
	void setPosition(double pos);

	#include "synthizer/property_methods.hpp"
	private:
	template<bool L>
	void readInterpolated(double pos, float *out);
	/* Adds to destination, per the generators API. */
	void generateNoPitchBend(float *out);
	template<bool L>
	void generatePitchBendHelper(float *out, double pitch_bend);
	void generatePitchBend(float *out, double pitch_bend);

	std::weak_ptr<Buffer> buffer;
	BufferReader reader;
	double position = 0.0;
	bool looping = false;
};

}
