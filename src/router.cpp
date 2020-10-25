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
		/* TODO: we need to prove that this can't happen. */
		if (route->input == nullptr) {
			continue;
		}

		float gain_start = route->fader.getValue(router->time);
		float gain_end = route->fader.getValue(router->time + 1);
		/*
		 * Relise on the guarantee that faders are "perfect" from an fp perspective outside their range.
		 * If this is ever not true, this code will still work, but will be very slow.
		 * */
		bool crossfading = gain_start != gain_end;
		if (crossfading) {
			for (unsigned int frame = 0; frame < config::BLOCK_SIZE; frame ++) {
				float w2 = frame / (float) config::BLOCK_SIZE;
				float w1 = 1.0f - w2;
				float gain = w1 * gain_start + w2 * gain_end;
				for (unsigned int channel = 0; channel < channels; channel++) {
					working_buf[frame * channels + channel] = gain * buffer[frame * channels + channel];
				}
			}
		} else {
			/* If we're here we might be able to just avoid doing it, if there's no gain. */
			if (gain_end == 0.0f) {
				continue;
			}

			/* Copy into working_buf, apply gain. */
			for (unsigned int f = 0; f < config::BLOCK_SIZE * channels; f++) {
				working_buf[f] = gain_end * buffer[f];
			}
		}
		mixChannels(config::BLOCK_SIZE, working_buf, channels, route->input->buffer, route->input->channels);
	}
}

Router::~Router() {
	for (auto i = this->routes.begin(); i != this->routes.end(); i++) {
		if (i->input) {
			i->input->router = nullptr;
		}
		if (i->output) {
			i->output->router = nullptr;
		}
	}
}

void Router::configureRoute(OutputHandle *output, InputHandle *input, float gain, unsigned int fade_blocks) {
	auto configured_iter = this->findRouteForPair(output, input);
	Route *to_configure;
	if (configured_iter != this->routes.end()) {
		to_configure = &*configured_iter;
	} else {
			Route key;
		key.output = output;
		key.input = input;
		auto insert_at = std::lower_bound(this->routes.begin(), this->routes.end(), key, [](const Route &x, const Route &y) {
			return compareRoutes(x, y) < 0;
		});
		auto inserted = this->routes.insert(insert_at, key);
		to_configure = &*inserted;
	}
	to_configure->output = output;
	to_configure->input = input;
	/*
	 * Replae the fader with one whose start gain is the current gain and whose end gain is the target.
	 * 
	 * This makes it so that a user hammering on the route configuration API should still get at least reasonable results.
	 * */
	float cgain = to_configure->fader.getValue(this->time);
	to_configure->fader = LinearFader(this->time, cgain, this->time + fade_blocks, gain);
}

void Router::removeRoute(OutputHandle *output, InputHandle *input, unsigned int fade_out) {
	auto iter = this->findRouteForPair(output, input);
	if (iter != this->routes.end()) {
		this->configureRoute(output, input, 0.0, fade_out);
	}
}

void Router::removeAllRoutes(OutputHandle *output, unsigned int fade_out) {
	auto i = this->findRun(output);
	while (i != this->routes.end() && i->output == output) {
		this->configureRoute(i->output, i->input, 0.0f, fade_out);
		i++;
	}
}

void Router::finishBlock() {
	this->time++;
	vector_helpers::filter_stable(this->routes, [&](auto &r) {
		bool dead = r.output == nullptr ||
			r.input == nullptr ||
			(r.fader.getValue(this->time) == 0.0 && r.fader.isFading(this->time) == false);
		if (dead) {
			return false;
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

SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle output, syz_Handle input, float fade_out) {
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
	unsigned int fade_out_blocks = fade_out * config::SR / config::BLOCK_SIZE;
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
