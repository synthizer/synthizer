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

InputHandle::InputHandle(Router *router, float *buffer, unsigned int channels) {
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

void OutputHandle::routeAudio(float *buffer, unsigned int channels) {
	alignas(config::ALIGNMENT)  thread_local std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> _working_buf;
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

		route->gain_driver.drive(router->time, [&](auto &gain_cb) {
			for (unsigned int frame = 0; frame < config::BLOCK_SIZE; frame ++) {
				float gain = gain_cb(frame);
				for (unsigned int channel = 0; channel < channels; channel++) {
					working_buf[frame * channels + channel] = gain * buffer[frame * channels + channel];
				}
			}
		});
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
	to_configure->gain_driver.setValue(this->time, gain);
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
			r.gain_driver.isActiveAtTime(this->time) == false;
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

SYZ_CAPI syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input, struct syz_RouteConfig *config) {
	(void)context;

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
	unsigned int fade_time  = config->fade_time * config::SR / config::BLOCK_SIZE;
	if (fade_time == 0 && config->fade_time != 0.0f) {
		// because the user asked for crossfade, but less than a block.
		fade_time = 1;
	}

	auto ctx = obj_input->getContext();
	ctx->enqueueReferencingCallbackCommand(true, [](auto &ctx, auto &obj_output, auto &obj_input, auto &gain, auto &fade_time) {
		if (ctx == nullptr) {
			return;
		}
		auto output_handle = obj_output->getOutputHandle();
		auto input_handle = obj_input->getInputHandle();
		auto r = ctx->getRouter();
		r->configureRoute(output_handle, input_handle, gain, fade_time);
	}, ctx, obj_output, obj_input, gain, fade_time);

	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, float fade_out) {
	(void)context;

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

	auto ctx = obj_input->getContext();
	ctx->enqueueReferencingCallbackCommand(true, [](auto &ctx, auto &obj_output, auto &obj_input, auto &fade_out_blocks) {
		if (ctx == nullptr) {
			return;
		}
		auto output_handle = obj_output->getOutputHandle();
		auto input_handle = obj_input->getInputHandle();
		ctx->getRouter()->removeRoute(output_handle, input_handle, fade_out_blocks);
	}, ctx, obj_output, obj_input, fade_out_blocks);

	return 0;
	SYZ_EPILOGUE
}
