#include "synthizer/generators/buffer.hpp"

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>

namespace synthizer {

unsigned int BufferGenerator::getChannels() {
	auto buffer = this->buffer.lock();
	if (buffer == nullptr) return 0;
	return this->reader.getChannels();
}

void BufferGenerator::generateBlock(AudioSample *output) {
	auto buffer = this->buffer.lock();
	if (buffer == nullptr || buffer->getLength() == 0) {
		return;
	} else if (abs(1.0 - this->pitch_bend) > 0.001) {
		return this->generatePitchBend(output);
	} else {
		return this->generateNoPitchBend(output);
	}
}

template<bool L>
void BufferGenerator::readInterpolated(double pos, AudioSample *out) {
	std::array<AudioSample, config::MAX_CHANNELS> f1, f2;
	std::size_t lower = std::floor(pos);
	std::size_t upper = lower + 1;
	if (L) upper = upper % this->reader.getLength();
	float w1 = upper - pos;
	float w2 = 1.0f - w1;
	this->reader.readFrame(lower, &f1[0]);
	this->reader.readFrame(upper, &f2[0]);
	for (unsigned int i = 0; i < this->reader.getChannels(); i++) {
		out[i] += f1[i] * w1 + f2[i] * w2;
	}
}

template<bool L>
void BufferGenerator::generatePitchBendHelper(AudioSample *output) {
	double pos = this->position;
	double delta = 1.0 / this->pitch_bend;
	for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
		this->readInterpolated<L>(pos, &output[i*this->reader.getChannels()]);
		pos += delta;
		if (L == true) pos = std::remainder(pos, this->reader.getLength());
		if (L == false && pos > this->reader.getLength()) break;
	}
	this->position = std::min<double>(pos, this->reader.getLength());
}

void BufferGenerator::generatePitchBend(AudioSample *output) {
	if (looping) {
		return this->generatePitchBendHelper<true>(output);
	} else {
		return this->generatePitchBendHelper<false>(output);
	}
}

void BufferGenerator::generateNoPitchBend(AudioSample *output) {
	alignas(config::ALIGNMENT) thread_local std::array<AudioSample, config::BLOCK_SIZE * config::MAX_CHANNELS> workspace = { 0.0f };
	std::size_t pos = std::round(this->position);
	AudioSample *cursor = output;
	unsigned int remaining = config::BLOCK_SIZE;
	while (remaining) {
		auto got = this->reader.readFrames(pos, remaining, &workspace[0]);
		for (unsigned int i = 0; i < got * this->getChannels(); i++) {
			cursor[i]  += workspace[i];
		}
		remaining -= got;
		cursor += got * this->reader.getChannels();
		pos += got;
		if (remaining > 0) {
			if (this->looping == false) break;
			else pos = 0;
		}
	}
	this->position = pos;
}

void BufferGenerator::setPitchBend(double newPitchBend) {
	this->pitch_bend = newPitchBend;
}

std::shared_ptr<Buffer> BufferGenerator::getBuffer() {
	return this->buffer.lock();
}

void BufferGenerator::setBuffer(const std::shared_ptr<Buffer> &b) {
	this->buffer = b;
	if (b != nullptr) {
		this->reader.setBuffer(b.get());
	}
	this->position = 0.0;
}

int BufferGenerator::getLooping() {
	return this->looping;
}

void BufferGenerator::setLooping(int l) {
	this->looping = l;
}

double BufferGenerator::getPosition() {
	return this->position / config::SR;
}

void BufferGenerator::setPosition(double pos) {
	double p = pos * config::SR;
	this->position = std::min<double>(p, this->reader.getLength());
}

}

#define PROPERTY_CLASS BufferGenerator
#define PROPERTY_BASE BaseObject
#define PROPERTY_LIST BUFFER_GENERATOR_PROPERTIES
#include "synthizer/property_impl.hpp"

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<BufferGenerator>();
	*out = toC(x);
	return 0;
	SYZ_EPILOGUE
}
