#include "synthizer/generators/buffer.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>

namespace synthizer {

unsigned int BufferGenerator::getChannels() {
	auto buf_weak = this->getBuffer();

	auto buffer = buf_weak.lock();
	if (buffer == nullptr) return 0;
	return buffer->getChannels();
}

void BufferGenerator::generateBlock(float *output, FadeDriver *gain_driver) {
	std::weak_ptr<Buffer> buffer_weak;
	std::shared_ptr<Buffer> buffer;
	bool buffer_changed = this->acquireBuffer(buffer_weak);
	double pitch_bend = this->getPitchBend();
	double new_pos;

	buffer = buffer_weak.lock();

	if (buffer == nullptr || buffer->getLength() == 0) {
		return;
	}
	if (buffer_changed) {
		this->configureBufferReader(buffer);
	}

	if (this->acquirePosition(new_pos)) {
		this->position_in_samples = std::min(new_pos * config::SR, (double)this->reader.getLength());
	}

	if (std::fabs(1.0 - pitch_bend) > 0.001) {
		this->generatePitchBend(output, gain_driver, pitch_bend);
	} else {
		this->generateNoPitchBend(output, gain_driver);
	}

	this->setPosition(this->position_in_samples / config::SR, false);
}

template<bool L>
void BufferGenerator::readInterpolated(double pos, float *out, float gain) {
	std::array<float, config::MAX_CHANNELS> f1, f2;
	std::size_t lower = std::floor(pos);
	std::size_t upper = lower + 1;
	if (L) upper = upper % this->reader.getLength();
	// Important: upper < lower if upper was past the last sample.
	float w2 = pos - lower;
	float w1 = 1.0 - w2;
	this->reader.readFrame(lower, &f1[0]);
	this->reader.readFrame(upper, &f2[0]);
	for (unsigned int i = 0; i < this->reader.getChannels(); i++) {
		out[i] += gain * (f1[i] * w1 + f2[i] * w2);
	}
}

template<bool L>
void BufferGenerator::generatePitchBendHelper(float *output, FadeDriver *gain_driver, double pitch_bend) {
	double pos = this->position_in_samples;
	double delta = pitch_bend;
	gain_driver->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
		for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
			float g = gain_cb(i);
			this->readInterpolated<L>(pos, &output[i*this->reader.getChannels()], g);
			pos += delta;
			if (L == true) pos = std::fmod(pos, this->reader.getLength());
			if (L == false && pos > this->reader.getLength()) break;
		}
	});
	this->position_in_samples = std::min<double>(pos, this->reader.getLength());
}

void BufferGenerator::generatePitchBend(float *output, FadeDriver *gain_driver, double pitch_bend) {
	if (this->getLooping()) {
		return this->generatePitchBendHelper<true>(output, gain_driver, pitch_bend);
	} else {
		return this->generatePitchBendHelper<false>(output, gain_driver, pitch_bend);
	}
}

void BufferGenerator::generateNoPitchBend(float *output, FadeDriver *gain_driver) {
	alignas(config::ALIGNMENT) thread_local std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> workspace = { 0.0f };
	std::size_t pos = std::round(this->position_in_samples);
	float *cursor = output;
	unsigned int remaining = config::BLOCK_SIZE;
	bool looping = this->getLooping() != 0;
	unsigned int i = 0;

	gain_driver->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
		while (remaining) {
			auto got = this->reader.readFrames(pos, remaining, &workspace[0]);
			for (unsigned int j = 0; j < got; i++, j++) {
				float g = gain_cb(i);
				for (unsigned int ch = 0; ch < this->reader.getChannels(); ch++) {
					cursor[i * this->reader.getChannels() + ch]  += g * workspace[i * this->reader.getChannels() + ch];
				}
			}
			remaining -= got;
			cursor += got * this->reader.getChannels();
			pos += got;
			if (remaining > 0) {
				if (looping == false) break;
				else pos = 0;
			}
		}
	});
	assert(i <= config::BLOCK_SIZE);

	this->position_in_samples = pos;
}

void BufferGenerator::configureBufferReader(const std::shared_ptr<Buffer> &b) {
	this->reader.setBuffer(b.get());
	this->setPosition(0.0);
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<BufferGenerator>();
	*out = toC(x);
	return 0;
	SYZ_EPILOGUE
}
