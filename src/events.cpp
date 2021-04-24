#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/context.hpp"
#include "synthizer/events.hpp"
#include "synthizer/memory.hpp"

#include <concurrentqueue.h>

#include <cassert>
#include <memory>
#include <utility>

namespace synthizer {

PendingEvent::PendingEvent() {}

PendingEvent::PendingEvent(syz_Event &&event, EventHandleVec &&referenced_handles): event(event), referenced_handles(referenced_handles), valid(true) {}

void PendingEvent::extract(syz_Event *out, unsigned long long flags) {
	*out = syz_Event{};

	for (std::size_t i = 0; i < this->referenced_handles.size(); i++) {
		auto strong = this->referenced_handles[i].lock();

		if (strong == nullptr || strong->incRef()) {
			/*
			 * We failed to revive this object and produce a strong reference. Decrement all the references before it in the vector.
			 * All the previous objects are still alive because we just incremented their reference counts, so unconditionally
			 * decrement their reference counts without checking again.  if this crashes,
			 * it's because the user misused the library.
			 * */
			for (std::size_t j = 0; j < i; j++) {
				this->referenced_handles[j].lock()->decRef();
			}
			return;
		}
	}

	*out = this->event;
	/* We only have flags late, so put them in now. */
	out->_private.flags = flags;
}

EventSender::EventSender(): pending_events(), producer_token(pending_events) {}

void EventSender::setEnabled(bool val) {
	this->enabled = val;
}

bool EventSender::isEnabled() {
	return this->enabled;
}

void EventSender::getNextEvent(syz_Event *out, unsigned long long flags) {
	PendingEvent maybe_event;

	*out = syz_Event{};

	if (this->pending_events.try_dequeue(maybe_event) == false) {
		return;
	}

	maybe_event.extract(out, flags);
}

void EventSender::enqueue(syz_Event &&event, EventHandleVec &&handles) {
	if (this->enabled == false) {
		return;
	}

	PendingEvent pending{std::move(event), std::move(handles)};
	this->pending_events.enqueue(this->producer_token, pending);
}

void EventBuilder::setSource(const std::shared_ptr<CExposable> &source) {
	if (this->associateObject(source)) {
		this->event.source = source->getCHandle();
		this->event.userdata = source->getUserdata();
		this->has_source = true;
	}
}

void EventBuilder::setContext(const std::shared_ptr<Context> &ctx) {
	auto base = std::static_pointer_cast<CExposable>(ctx);
	this->event.context = this->translateHandle(base);
}

syz_Handle EventBuilder::translateHandle(const std::shared_ptr<CExposable> &object) {
	if (this->associateObject(object) == false) {
		this->will_send = false;
		return 0;
	}
	return object->getCHandle();
}

syz_Handle EventBuilder::translateHandle(const std::weak_ptr<CExposable> &object) {
	if (object.expired() == true) {
		this->will_send = false;
		return 0;
	}
	return this->translateHandle(object.lock());
}

#define SP(E, T, F) \
void EventBuilder::setPayload(const T &payload) { \
	assert(this->has_payload == false && "Events may only have one payload"); \
	this->event.payload.F = payload; \
	this->event.type = E; \
	this->has_payload = true; \
}

SP(SYZ_EVENT_TYPE_LOOPED, syz_EventLooped, looped)
SP(SYZ_EVENT_TYPE_FINISHED, syz_EventFinished, finished)

void EventBuilder::dispatch(EventSender *sender) {
	if (this->will_send == false) {
		return;
	}

	assert(this->has_source && "Events must have sources");
	assert(this->has_payload && "Events must have payloads");

	sender->enqueue(std::move(this->event), std::move(this->referenced_objects));
}

bool EventBuilder::associateObject(const std::shared_ptr<CExposable> &obj) {
	if (obj == nullptr || obj->isPermanentlyDead()) {
		return false;
	}
	auto weak = std::weak_ptr<CExposable>(obj);
	assert(this->referenced_objects.pushBack(std::move(weak)) && "Event has too many referenced objects");
	return true;
}

void sendFinishedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source) {
	ctx->sendEvent([&](auto *builder) {
		builder->setSource(std::static_pointer_cast<CExposable>(source));
		builder->setContext(ctx);
		builder->setPayload(syz_EventFinished{});
	});
}

void sendLoopedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source) {
	ctx->sendEvent([&](auto *builder) {
		builder->setSource(std::static_pointer_cast<CExposable>(source));
		builder->setContext(ctx);
		builder->setPayload(syz_EventLooped{});
	});
}

}



SYZ_CAPI void syz_eventDeinit(struct syz_Event *event) {
	/*
	 * Deinitialize an event by decrementing reference counts as necessary. We'll just
	 * go through the C API and not check errors: syz_handleDecRef always succeeds save for programmer error.
	 * */
	if (event->_private.flags & SYZ_EVENT_FLAG_TAKE_OWNERSHIP) {
		return;
	}

	/* Invalid events are zero-initialized, so there's no need to check. */
	syz_handleDecRef(event->source);
	syz_handleDecRef(event->context);
}
