#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/generators/buffer.hpp"

#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/events.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>

namespace synthizer {

BufferGenerator::BufferGenerator(std::shared_ptr<Context> ctx): Generator(ctx) {
}

int BufferGenerator::getObjectType() {
	return SYZ_OTYPE_BUFFER_GENERATOR;
}

unsigned int BufferGenerator::getChannels() {
	auto buf_weak = this->getBuffer();

	auto buffer = buf_weak.lock();
	if (buffer == nullptr) return 0;
	return buffer->getChannels();
}

void BufferGenerator::generateBlock(float *output, FadeDriver *gd) {
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

	if (this->acquirePlaybackPosition(new_pos)) {
		this->position_in_samples = std::min(new_pos * config::SR, (double)this->reader.getLength());
		this->sent_finished = false;
	}

	if (std::fabs(1.0 - pitch_bend) > 0.001) {
		this->generatePitchBend(output, gd, pitch_bend);
	} else {
		this->generateNoPitchBend(output, gd);
	}

	this->setPlaybackPosition(this->position_in_samples / config::SR, false);

	while (this->looped_count > 0) {
		sendLoopedEvent(this->getContext(), this->shared_from_this());
		this->looped_count--;
	}
	while (this->finished_count > 0) {
		sendFinishedEvent(this->getContext(), this->shared_from_this());
		this->finished_count--;
	}
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
void BufferGenerator::generatePitchBendHelper(float *output, FadeDriver *gd, double pitch_bend) {
	double pos = this->position_in_samples;
	double delta = pitch_bend;
	gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
		for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
			float g = gain_cb(i);
			this->readInterpolated<L>(pos, &output[i*this->reader.getChannels()], g);
			double new_pos = pos + delta;
			if (L == true) {
				new_pos = std::fmod(new_pos, this->reader.getLength());
				if (new_pos < pos) {
					this->looped_count += 1;
				}
			} else if (pos > this->reader.getLength()) {
				if ( this->sent_finished == false) {
					/* Don't forget to guard against sending this multiple times. */
					this->finished_count += 1;
					this->sent_finished = true;
				}
				break;
			}
			pos = new_pos;
		}
	});
	this->position_in_samples = std::min<double>(pos, this->reader.getLength());
}

void BufferGenerator::generatePitchBend(float *output, FadeDriver *gd, double pitch_bend) {
	if (this->getLooping()) {
		return this->generatePitchBendHelper<true>(output, gd, pitch_bend);
	} else {
		return this->generatePitchBendHelper<false>(output, gd, pitch_bend);
	}
}

void BufferGenerator::generateNoPitchBend(float *output, FadeDriver *gd) {
	auto workspace_guard = acquireBlockBuffer();
	float *workspace = workspace_guard;
	std::size_t pos = std::round(this->position_in_samples);
	float *cursor = output;
	unsigned int remaining = config::BLOCK_SIZE;
	bool looping = this->getLooping() != 0;
	unsigned int i = 0;

	gd->drive(this->getContextRaw()->getBlockTime(), [&](auto &gain_cb) {
		while (remaining) {
			auto got = this->reader.readFrames(pos, remaining, workspace);
			for (unsigned int j = 0; j < got; i++, j++) {
				float g = gain_cb(i);
				for (unsigned int ch = 0; ch < this->reader.getChannels(); ch++) {
					cursor[j * this->reader.getChannels() + ch]  += g * workspace[j * this->reader.getChannels() + ch];
				}
			}
			remaining -= got;
			cursor += got * this->reader.getChannels();
			pos += got;
			if (remaining > 0) {
				if (looping == false) {
					if (this->sent_finished == false) {
						this->finished_count += 1;
						this->sent_finished = true;
					}
					break;
				} else {
					pos = 0;
					this->looped_count += 1;
				}
			}
		}
	});
	assert(i <= config::BLOCK_SIZE);

	this->position_in_samples = pos;
}

void BufferGenerator::configureBufferReader(const std::shared_ptr<Buffer> &b) {
	this->reader.setBuffer(b.get());
	this->setPlaybackPosition(0.0);
}

void BufferGenerator::handleEndEvent() {
	auto ctx = this->getContext();
	if (this->getLooping() == 1) {
		sendLoopedEvent(ctx, this->shared_from_this());
	} else {
		sendFinishedEvent(ctx, this->shared_from_this());
	}
}

std::optional<double> BufferGenerator::startGeneratorLingering() {
	/**
	 * To linger, stop any looping, then set the timeout to the duration of the buffer
	 * minus the current position.
	 * */
	double pos = this->getPlaybackPosition();
	this->setLooping(false);
	auto buf = this->getBuffer();
	auto buf_strong = buf.lock();
	if (buf_strong == nullptr) {
		return 0.0;
	}
	double remaining = buf_strong->getLength() / (double)config::SR - pos;
	if (remaining < 0.0) {
		return 0.0;
	}
	auto pb = this->getPitchBend();
	return remaining / pb;
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context, void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
	SYZ_PROLOGUE
	auto ctx = fromC<Context>(context);
	auto x = ctx->createObject<BufferGenerator>();
	*out = toC(x);
	return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
	SYZ_EPILOGUE
}
