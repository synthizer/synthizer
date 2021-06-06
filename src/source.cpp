#include "synthizer.h"

#include "synthizer/sources.hpp"

#include "synthizer/biquad.hpp"
#include "synthizer/block_buffer_cache.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/math.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace synthizer {

void Source::addGenerator(std::shared_ptr<Generator> &generator) {
	if (this->hasGenerator(generator)) return;
	this->generators.emplace_back(generator);
}

void Source::removeGenerator(std::shared_ptr<Generator> &generator) {
	if (this->generators.empty()) return;
	if (this->hasGenerator(generator) == false) return;

	unsigned int index = 0;
	for(; index < this->generators.size(); index++) {
		auto s = this->generators[index].lock();
		if (s == generator) break;
	}

	std::swap(this->generators[this->generators.size()-1], this->generators[index]);
	this->generators.resize(this->generators.size() - 1);
}

bool Source::hasGenerator(std::shared_ptr<Generator> &generator) {
	return weak_vector::contains(this->generators, generator);
}

void Source::fillBlock(unsigned int channels) {
	auto premix_guard = acquireBlockBuffer();
	float *premix = premix_guard;
	double gain_prop;
	auto time = this->context->getBlockTime();

	if (channels != this->last_channels) {
		this->filter = createBiquadFilter(channels);
		this->filter_direct = createBiquadFilter(channels);
		this->filter_effects = createBiquadFilter(channels);
		this->filter->configure(this->getFilter());
		this->filter_direct->configure(this->getFilterDirect());
		this->filter_effects->configure(this->getFilterEffects());
		this->last_channels = channels;
	}

	if (this->acquireGain(gain_prop) || this->shouldIncorporatePausableGain()) {
		gain_prop *= this->getPausableGain();
		this->gain_fader.setValue(time, gain_prop);
	}

	std::fill(&this->block[0], &this->block[0] + channels * config::BLOCK_SIZE, 0.0f);

	/**
	 * There is room for further optimization here, by communicating that this block is potentially zeros
	 * to the derived source classes. We'll do that later, when we also have tracking of generator
	 * silence and other scheduling related functionality that makes it advantageous for the other sources to drop their panners and so on.
	 * */
	if (this->isPaused()) {
		return;
	}
	this->tickPausable();

	/* iterate and remove as we go to avoid locking weak_ptr twice. */
	weak_vector::iterate_removing(this->generators, [&] (auto &g) {
		unsigned int nch = g->getChannels();

		if (nch == 0) {
			return;
		}

		if (nch == channels) {
			g->run(&this->block[0]);
		} else {
			std::fill(premix, premix + config::BLOCK_SIZE * nch, 0.0f);
			g->run(premix);
			mixChannels(config::BLOCK_SIZE, premix, nch, &this->block[0], channels);
		}
	});

	this->gain_fader.drive(time, [&](auto &gain_cb) {
		for (unsigned int i =  0; i < config::BLOCK_SIZE; i++) {
			float g = gain_cb(i);
			for (unsigned int ch = 0; ch < channels; ch++) {
				this->block[i * channels + ch] *= g;
			}
		}
	});


	struct syz_BiquadConfig filter_cfg;
	if (this->acquireFilter(filter_cfg)) {
		this->filter->configure(filter_cfg);
	}
	if (this->acquireFilterDirect(filter_cfg)) {
		this->filter_direct->configure(filter_cfg);
	}
	if (this->acquireFilterEffects(filter_cfg)) {
		this->filter_effects->configure(filter_cfg);
	}

	/*
	 * Filter goes to both paths, so run it first.
	 * */
	this->filter->processBlock(&this->block[0], &this->block[0], false);

	{
		auto buf_guard = acquireBlockBuffer(false);
		this->filter_effects->processBlock(&this->block[0], buf_guard, false);
		this->getOutputHandle()->routeAudio(buf_guard, channels);
	}

	/*
	 * And now the direct filter, which applies only to the source's local block.
	 * */
	this->filter_direct->processBlock(&this->block[0], &this->block[0], false);

	if (this->is_lingering && this->generators.empty()) {
		this->linger_countdown--;
		if (this->linger_countdown == 0) {
			this->signalLingerStopPoint();
		}
	}
}

bool Source::wantsLinger() {
	return true;
}

std::optional<double> Source::startLingering(const std::shared_ptr<CExposable> &reference, double configured_timeout) {
	CExposable::startLingering(reference, configured_timeout);

	this->is_lingering = true;
	/**
	 * Sources linger until they have no generators.
	 * */
	return std::nullopt;
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator) {
	SYZ_PROLOGUE
	auto src = fromC<Source>(source);
	auto gen = fromC<Generator>(generator);
	src->getContextRaw()->enqueueReferencingCallbackCommand(true, [] (auto &src, auto &gen) {
		src->addGenerator(gen);
	}, src, gen);
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator) {
	SYZ_PROLOGUE
	auto src = fromC<Source>(source);
	auto gen = fromC<Generator>(generator);
	src->getContextRaw()->enqueueReferencingCallbackCommand(true, [] (auto &src, auto &gen) {
		src->removeGenerator(gen);
	}, src, gen);
	return 0;
	SYZ_EPILOGUE
}
