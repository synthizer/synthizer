#include "synthizer/router.hpp"

#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/types.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <cassert>
#include <tuple>

namespace synthizer {

using namespace router;

/* Like strcmp but for Route. */
int compareRoutes(const Route &a, const Route &b) {
	const std::tuple<const WriterHandle *, const ReaderHandle *> k1{a.writer, a.reader};
	const std::tuple<const WriterHandle *, const ReaderHandle *> k2{b.writer, b.reader};
	if (k1 == k2) {
		return 0;
	}
	return k1 < k2 ? -1 : 1;
}

ReaderHandle::ReaderHandle(Router *router, AudioSample *buffer, unsigned int channels) {
	this->router = router;
	this->buffer = buffer;
	this->channels = channels;
}

ReaderHandle::~ReaderHandle() {
	if (this->router) router->unregisterReaderHandle(this);
}

WriterHandle::WriterHandle(Router *router) {
	this->router = router;
}

WriterHandle::~WriterHandle() {
	if (router) this->router->unregisterWriterHandle(this);
}

void WriterHandle::routeAudio(AudioSample *buffer, unsigned int channels) {
	alignas(config::ALIGNMENT)  thread_local std::array<AudioSample, config::BLOCK_SIZE * config::MAX_CHANNELS> _working_buf;
	auto *working_buf = &_working_buf[0];
	auto start = this->router->findRun(this);
	if (start == router->routes.end()) {
		/* No routes for this writer handle. */
		return;
	}
	for (auto i = start; i < router->routes.end() && i->writer == this; i++) {
		if (i->state == RouteState::Dead) {
			continue;
		}
		/* Work out if we have to crossfade, and if so, by what. */
		float gain_start = i->gain, gain_end = i->gain;
		unsigned int blocks = 0;
		if (i->state == RouteState::FadeIn) {
			gain_start = 0.0f;
			blocks = i->fade_in_blocks;
		} else if (i->state == RouteState::FadeOut) {
			gain_end = 0.0f;
			blocks = i->fade_out_blocks;
		} else if (i->state == RouteState::GainChanged) {
			gain_start = i->prev_gain;
			blocks = 1;
		}

		bool crossfading = blocks > 0 && i->state != RouteState::Steady;
		if (crossfading) {
			/*
			 * explanation: there is a line from gain_start to gain_end, over blocks blocks.
			 * We want to figure out the start and end portions of this line for the block we currently need to do.
			 * To do so, build the equation of the line, figure out how many blocks we've done, and evaluate there plus 1 block in the future.
			 * gain_start is effectively the y axis, if we shift things so that last_state_changed == 0.
			 * */
			float slope = (gain_end - gain_start) / blocks;
			auto done = this->router->time - i->last_state_changed;
			gain_end = slope * (done + 1) + gain_start;
			gain_start = slope * done + gain_start;
			for (unsigned int frame = 0; frame < config::BLOCK_SIZE; frame ++) {
				float w2 = frame / (float) config::BLOCK_SIZE;
				float w1 = 1.0f - w2;
				float gain = w1 * gain_start + w2 * gain_end;
				for (unsigned int channel = 0; channel < channels; channel++) {
					working_buf[frame * channel + channels] = gain * buffer[frame * channels + channel];
				}
			}
		} else {
			/* Copy into working_buf, apply gain. */
			for (unsigned int f = 0; f < config::BLOCK_SIZE * channels; i++) {
				working_buf[f] = i->gain * buffer[f];
			}
		}
		mixChannels(config::BLOCK_SIZE, working_buf, channels, i->reader->buffer, i->reader->channels);
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
		if (i->reader) {
			i->reader->router = nullptr;
		}
		if (i->writer) {
			i->writer->router = nullptr;
		}
	}
}

void Router::configureRoute(WriterHandle *w, ReaderHandle *r, float gain, unsigned int fade_in) {
	auto configured_iter = this->findRouteForPair(w, r);
	if (configured_iter != this->routes.end() && configured_iter->canConfigure()) {
		configured_iter->setGain(gain, this->time);
		return;
	}
	/* Otherwise, we need to set up a new route. */
	Route nr;
	nr.writer = w;
	nr.reader = r;
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

void Router::removeRoute(WriterHandle *w, ReaderHandle *r, unsigned int fade_out) {
	auto iter = this->findRouteForPair(w, r);
	if (iter != this->routes.end()) {
		deprovisionRoute(iter, fade_out, this->time);
	}
}

void Router::removeAllRoutes(WriterHandle *w, unsigned int fade_out) {
	auto i = this->findRun(w);
	while (i != this->routes.end() && i->writer == w) {
		deprovisionRoute(i, fade_out, this->time);
		i++;
	}
}

void Router::finishBlock() {
	this->time++;
	vector_helpers::filter_stable(this->routes, [&](auto &r) {
		if (r.reader == nullptr || r.writer == nullptr) {
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
		return true;
	});
}

void Router::unregisterWriterHandle(WriterHandle *w) {
	vector_helpers::filter_stable(this->routes, [&](auto &r) {
		return r.writer != w;
	});
}

void Router::unregisterReaderHandle(ReaderHandle *r) {
	vector_helpers::filter_stable(this->routes, [&](auto &ro) {
		return ro.reader != r;
	});
}

deferred_vector<Route>::iterator Router::findRouteForPair(WriterHandle *w, ReaderHandle *r) {
	Route key;
	key.writer = w;
	key.reader = r;
	auto iter = std::lower_bound(this->routes.begin(), this->routes.end(), key, [](const Route &a, const Route &b) {
		return compareRoutes(a, b) < 0;
	});
	if (iter == this->routes.end()) {
		return iter;
	}
	if (iter->writer == w && iter->reader == r) {
		return iter;
	}
	return this->routes.end();
}

deferred_vector<Route>::iterator Router::findRun(WriterHandle *w) {
	Route key;
	key.writer = w;
	key.reader = nullptr;
	auto i = std::lower_bound(this->routes.begin(), this->routes.end(), key, [](const Route &a, const Route &b) {
		return compareRoutes(a, b) < 0;
	});
	if (i == this->routes.end() || i->writer == w) {
		return i;
	}
	return this->routes.end();
}

}