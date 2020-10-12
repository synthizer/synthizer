#include "synthizer/effects/echo.hpp"

#include "synthizer/channel_mixing.hpp"

#include <array>

namespace synthizer {

template<bool FADE_IN, bool ADD>
void EchoEffect::runEffectInternal(AudioSample *output) {
	this->line.runReadLoop(this->max_delay_tap, [&](unsigned int i, auto &reader) {
		for (auto &t: this->taps) {
			float l = reader.read(0, t.config.delay);
			float r = reader.read(1, t.config.delay);
			if (FADE_IN) {
				float g = i / (float)config::BLOCK_SIZE;
				l *= g;
				r *= g;
			}
			if (ADD) {
				output[i * 2] += l;
				output[i * 2 + 1] += r;
			} else {
				output[i * 2] = l;
				output[i * 2 + 1] = r;
			}
		}
	});
}

void EchoEffect::runEffect(unsigned int time_in_blocks, unsigned int input_channels, AudioSample *input, unsigned int output_channels, AudioSample *output) {
	thread_local std::array<AudioSample, config::BLOCK_SIZE * 2> working_buf;

	auto *buffer = this->line.getNextBlock();
	/* Mix data to stereo. */
	mixChannels(config::BLOCK_SIZE, input, input_channels, buffer, 2);

	/* maybe apply the new config. */
	deferred_vector<EchoTapConfig> new_config;
	bool will_crossfade = this->pending_configs.try_dequeue(new_config);
	/* Drain any stale values. When this is moved to a proper box, this will be handled for us. */
	while (this->pending_configs.try_dequeue(new_config));
	if (will_crossfade) {
		this->taps.clear();
		this->max_delay_tap = 0;
		for (auto &c: new_config) {
			EchoTap tap;
			tap.config = c;
			this->taps.push_back(tap);
			this->max_delay_tap = this->max_delay_tap > c.delay ? this->max_delay_tap : c.delay;
		}
	}

	if (output_channels != 2) {
		if (will_crossfade) {
			this->runEffectInternal<true, false>(&working_buf[0]);
		} else {
			this->runEffectInternal<false, false>(&working_buf[0]);
		}
		mixChannels(config::BLOCK_SIZE, &working_buf[0], 2, output, output_channels);
	} else {
		if (will_crossfade) {
			this->runEffectInternal<true, true>(output);
		} else {
			this->runEffectInternal<false, true>(output);
		}
	}
}

void EchoEffect::resetEffect() {
	this->line.clear();
}

}
