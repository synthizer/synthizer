#include "synthizer/router.hpp"

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
	/* TODO. */
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
	/* Maybe filter out any dead routes. */
	if (this->time % FILTER_BLOCK_COUNT == 0) {
		this->filterRoutes(nullptr, nullptr);
	}
}

void Router::unregisterWriterHandle(WriterHandle *w) {
	this->filterRoutes(w, nullptr);
}

void Router::unregisterReaderHandle(ReaderHandle *r) {
	this->filterRoutes(nullptr, r);
}

void Router::filterRoutes(WriterHandle *w, ReaderHandle *r) {
	vector_helpers::filter_stable(this->routes, [w, r](const Route &i) {
		return i.state == RouteState::Dead || i.writer == w || i.reader == r;
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