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
	void generateBlock(AudioSample *output) override;
	void setPitchBend(double newPitchBend) override;
	std::shared_ptr<Buffer> getBuffer();
	void setBuffer(const std::shared_ptr<Buffer> &buffer);
	int getLooping();
	void setLooping(int l);
	double getPosition();
	void setPosition(double pos);

	PROPERTY_METHODS;
	private:
	template<bool L>
	void readInterpolated(double pos, AudioSample *out);
	/* Adds to destination, per the generators API. */
	void generateNoPitchBend(AudioSample *out);
	template<bool L>
	void generatePitchBendHelper(AudioSample *out);
	void generatePitchBend(AudioSample *out);

	std::shared_ptr<Buffer> buffer = nullptr;

	BufferReader reader;
	double pitch_bend = 1.0;
	double position = 0.0;
	bool looping = false;
};

}
