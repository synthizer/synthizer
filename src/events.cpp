#include "synthizer_constants.h"
#include "synthizer_events.h"

#include "synthizer/events.hpp"

#include "synthizer/memory.hpp"

#include "concurrentqueue.h"

#include <cassert>
#include <memory>
#include <utility>

namespace synthizer {

static bool hasValidCHandle(const std::shared_ptr<CExposable> &obj) {
	return obj != nullptr && obj->isPermanentlyDead() == false;
}

static bool hasValidCHandle(const std::weak_ptr<CExposable> &obj) {
	return obj.expired() == false && hasValidCHandle(obj.lock());
}

PendingEvent::PendingEvent() {}

PendingEvent::PendingEvent(syz_Event &&event, EventHandleVec &&referenced_handles): event(event), referenced_handles(referenced_handles), valid(true) {}

bool PendingEvent::extract(syz_Event *out) {
	*out = syz_Event{};

	for (auto &o: this->referenced_handles) {
		if (hasValidCHandle(o) == false) {
			return false;
		}
	}

	*out = this->event;
	return true;
}

EventSender::EventSender(): pending_events(), producer_token(pending_events) {}

void EventSender::setEnabled(bool val) {
	this->enabled = val;
}

bool EventSender::getNextEvent(syz_Event *out) {
	PendingEvent maybe_event;

	*out = syz_Event{};

	if (this->pending_events.try_dequeue(maybe_event)) {
		return false;
	}

	return maybe_event.extract(out);
}

void EventSender::enqueue(syz_Event &&event, EventHandleVec &&handles) {
	if (this->enabled == false) {
		return;
	}

	PendingEvent pending{std::move(event), std::move(handles)};
	assert(this->pending_events.enqueue(this->producer_token, std::move(pending)) == true);
}

void EventBuilder::setSource(const std::shared_ptr<CExposable> &source) {
	if (this->associateObject(source)) {
		this->event.source = source->getCHandle();
		this->event.userdata = source->getUserdata();
		this->has_source = true;
	}
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
	if (hasValidCHandle(obj) == false) {
		return false;
	}
	auto weak = std::weak_ptr(obj);
	assert(this->referenced_objects.pushBack(std::move(weak)) && "Event has too many referenced objects");
	return true;
}

}
