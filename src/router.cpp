#include "synthizer/router.hpp"

#include "synthizer.h"

#include "synthizer/base_object.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/context.hpp"
#include "synthizer/error.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <cassert>
#include <tuple>

namespace synthizer {

using namespace router;

/* Like strcmp but for Route. */
int compareRoutes(const Route &a, const Route &b) {
	const std::tuple<const OutputHandle *, const InputHandle *> k1{a.output, a.input};
	const std::tuple<const OutputHandle *, const InputHandle *> k2{b.output, b.input};
	if (k1 == k2) {
		return 0;
	}
	return k1 < k2 ? -1 : 1;
}

InputHandle::InputHandle(Router *router, AudioSample *buffer, unsigned int channels) {
	this->router = router;
	this->buffer = buffer;
	this->channels = channels;
}

InputHandle::~InputHandle() {
	if (this->router) router->unregisterInputHandle(this);
}

OutputHandle::OutputHandle(Router *router) {
	this->router = router;
}

OutputHandle::~OutputHandle() {
	if (router) this->router->unregisterOutputHandle(this);
}

void OutputHandle::routeAudio(AudioSample *buffer, unsigned int channels) {
	alignas(config::ALIGNMENT)  thread_local std::array<AudioSample, config::BLOCK_SIZE * config::MAX_CHANNELS> _working_buf;
	auto *working_buf = &_working_buf[0];
	auto start = this->router->findRun(this);
	if (start == router->routes.end()) {
		return;
	}
	for (auto route = start; route < router->routes.end() && route->output == this; route++) {
		if (route->state == RouteState::Dead) {
			continue;
		}
		/* Work out if we have to crossfade, and if so, by what. */
		float gain_start = route->gain, gain_end = route->gain;
		unsigned int blocks = 0;
		if (route->state == RouteState::FadeIn) {
			gain_start = 0.0f;
			blocks = route->fade_in_blocks;
		} else if (route->state == RouteState::FadeOut) {
			gain_end = 0.0f;
			blocks = route->fade_out_blocks;
		} else if (route->state == RouteState::GainChanged) {
			gain_start = route->prev_gain;
			blocks = 1;
		}

		bool crossfading = blocks > 0 && route->state != RouteState::Steady;
		if (crossfading) {
			/*
			 * explanation: there is a line from gain_start to gain_end, over blocks blocks.
			 * We want to figure out the start and end portions of this line for the block we currently need to do.
			 * To do so, build the equation of the line, figure out how many blocks we've done, and evaluate there plus 1 block in the future.
			 * gain_start is effectively the y axis, if we shift things so that last_state_changed == 0.
			 * */
			float slope = (gain_end - gain_start) / blocks;
			auto done = this->router->time - route->last_state_changed;
			gain_end = slope * (done + 1) + gain_start;
			gain_start = slope * done + gain_start;
			for (unsigned int frame = 0; frame < config::BLOCK_SIZE; frame ++) {
				float w2 = frame / (float) config::BLOCK_SIZE;
				float w1 = 1.0f - w2;
				float gain = w1 * gain_start + w2 * gain_end;
				for (unsigned int channel = 0; channel < channels; channel++) {
					working_buf[frame * channels + channel] = gain * buffer[frame * channels + channel];
				}
			}
		} else {
			/* Copy into working_buf, apply gain. */
			for (unsigned int f = 0; f < config::BLOCK_SIZE * channels; f++) {
				working_buf[f] = route->gain * buffer[f];
			}
		}
		mixChannels(config::BLOCK_SIZE, working_buf, channels, route->input->buffer, route->input->channels);
	}
}

bool Route::canConfigure() {
	return this->state != RouteState::Dead && this->state != RouteState::FadeOut;
}

void Route::setGain(float gain, unsigned int time_block) {
	if (this->state == RouteState::FadeIn || this->state == RouteState::GainChanged) {
		this->gain = gain;
		return;
	}
	this->prev_gain = this->gain;
	this->gain = gain;
	this->setState(RouteState::GainChanged, time_block);
}

void Route::setState(RouteState state, unsigned int time_block) {
	this->state = state;
	this->last_state_changed = time_block;
}

Router::~Router() {
	for (auto i = this->routes.begin(); i != this->routes.end(); i++) {
		if (i->state == RouteState::Dead) {
			continue;
		}
		if (i->input) {
			i->input->router = nullptr;
		}
		if (i->output) {
			i->output->router = nullptr;
		}
	}
}

void Router::configureRoute(OutputHandle *output, InputHandle *input, float gain, unsigned int fade_in) {
	auto configured_iter = this->findRouteForPair(output, input);
	if (configured_iter != this->routes.end() && configured_iter->canConfigure()) {
		configured_iter->setGain(gain, this->time);
		return;
	}
	/* Otherwise, we need to set up a new route. */
	Route nr;
	nr.output = output;
	nr.input = input;
	nr.fade_in_blocks = fade_in;
	nr.setState(fade_in == 0 ? RouteState::Steady : RouteState::FadeIn, this->time);
	/* Figure out where in the vector to insert it. */
	auto insert_at = std::lower_bound(this->routes.begin(), this->routes.end(), nr, [](const Route &x, const Route &y) {
		return compareRoutes(x, y) < 0;
	});
	this->routes.insert(insert_at, nr);
}

template<typename I>
static void deprovisionRoute(I &&iter, unsigned int fade_out, unsigned int time_block) {
	if (iter->canConfigure()) {
		iter->setState(fade_out != 0 ? RouteState::FadeOut : RouteState::Dead, time_block);
		iter->fade_out_blocks = fade_out;
	}
}

void Router::removeRoute(OutputHandle *output, InputHandle *input, unsigned int fade_out) {
	auto iter = this->findRouteForPair(output, input);
	if (iter != this->routes.end()) {
		deprovisionRoute(iter, fade_out, this->time);
	}
}

void Router::removeAllRoutes(OutputHandle *output, unsigned int fade_out) {
	auto i = this->findRun(output);
	while (i != this->routes.end() && i->output == output) {
		deprovisionRoute(i, fade_out, this->time);
		i++;
	}
}

void Router::finishBlock() {
	this->time++;
	vector_helpers::filter_stable(this->routes, [&](auto &r) {
		if (r.output == nullptr || r.input == nullptr) {
			return false;
		}
		/* Advance the state machine for this route. */
		auto delta = this->time - r.last_state_changed;
		if (r.state == RouteState::FadeIn && delta >= r.fade_in_blocks) {
			r.state = RouteState::Steady;
		} else if (r.state == RouteState::FadeOut && delta > r.fade_out_blocks) {
			r.state = RouteState::Dead;
			return false;
		} else if (r.state == RouteState::GainChanged && delta == 1) {
			r.state = RouteState::Steady;
		}
		/* Zero the input buffer for this route, if any. */
		if (r.input && r.input->buffer) {
			std::fill(r.input->buffer, r.input->buffer + config::BLOCK_SIZE * r.input->channels, 0.0f);
		}
		return true;
	});
}

void Router::unregisterOutputHandle(OutputHandle *output) {
	vector_helpers::filter_stable(this->routes, [&](auto &r) {
		return r.output != output;
	});
}

void Router::unregisterInputHandle(InputHandle *input) {
	vector_helpers::filter_stable(this->routes, [&](auto &r) {
		return r.input != input;
	});
}

deferred_vector<Route>::iterator Router::findRouteForPair(OutputHandle *output, InputHandle *input) {
	Route key;
	key.output = output;
	key.input = input;
	auto iter = std::lower_bound(this->routes.begin(), this->routes.end(), key, [](const Route &a, const Route &b) {
		return compareRoutes(a, b) < 0;
	});
	if (iter == this->routes.end()) {
		return iter;
	}
	if (iter->output == output && iter->input == input) {
		return iter;
	}
	return this->routes.end();
}

deferred_vector<Route>::iterator Router::findRun(OutputHandle *output) {
	Route key;
	key.output = output;
	key.input = nullptr;
	auto i = std::lower_bound(this->routes.begin(), this->routes.end(), key, [](const Route &a, const Route &b) {
		return compareRoutes(a, b) < 0;
	});
	if (i == this->routes.end() || i->output == output) {
		return i;
	}
	return this->routes.end();
}

}


using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_routingEstablishRoute(syz_Handle output, syz_Handle input, struct RouteConfig *config) {
	SYZ_PROLOGUE
	auto obj_output = fromC<BaseObject>(output);
	auto obj_input = fromC<BaseObject>(input);
	if (obj_output->getContextRaw() != obj_input->getContextRaw()) {
		throw EInvariant("Objects are not from the same context");
	}
	auto output_handle = obj_output->getOutputHandle();
	if (output_handle == nullptr) {
		throw EInvariant("Output doesn't support routing to inputs");
	}
	auto input_handle = obj_input->getInputHandle();
	if (input_handle == nullptr) {
		throw EInvariant("Input doesn't support connecting to outputs");
	}
	float gain = config->gain;
	unsigned int fade_in  = config->fade_in * config::SR / config::BLOCK_SIZE;
	if (fade_in == 0 && config->fade_in != 0.0f) {
		// because the user asked for crossfade, but less than a block.
		fade_in = 1;
	}
	auto ctx = obj_input->getContextRaw();
	ctx->call([&]() {
		auto r = ctx->getRouter();
		r->configureRoute(output_handle, input_handle, gain, fade_in);
	});
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle input, syz_Handle output, float fade_out) {
	SYZ_PROLOGUE
	auto obj_output = fromC<BaseObject>(output);
	auto obj_input = fromC<BaseObject>(input);
	if (obj_output->getContextRaw() != obj_input->getContextRaw()) {
		throw EInvariant("Objects are not from the same context");
	}
	auto output_handle = obj_output->getOutputHandle();
	if (output_handle == nullptr) {
		throw EInvariant("Output doesn't support routing to inputs");
	}
	auto input_handle = obj_input->getInputHandle();
	if (input_handle == nullptr) {
		throw EInvariant("Input doesn't support connecting to outputs");
	}
	unsigned int fade_out_blocks = fade_out * config::SR;
	if (fade_out != 0.0f && fade_out_blocks == 0) {
		fade_out_blocks = 1;
	}
	auto ctx = obj_input->getContextRaw();
	ctx->call([&]() {
		ctx->getRouter()->removeRoute(output_handle, input_handle, fade_out_blocks);
	});
	return 0;
	SYZ_EPILOGUE
}
