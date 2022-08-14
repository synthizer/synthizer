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

PendingEvent::PendingEvent(syz_Event &&event, EventHandleVec &&referenced_handles)
    : event(event), referenced_handles(referenced_handles), valid(true) {}

void PendingEvent::extract(syz_Event *out) {
  *out = syz_Event{};

  for (std::size_t i = 0; i < this->referenced_handles.size(); i++) {
    auto strong = this->referenced_handles[i].lock();

    if (strong == nullptr || strong->incRef() == false) {
      /**
       * We failed to revive this object and produce a strong reference.
       * Decrement all the references before it in the vector. All the
       * previous objects are still alive because we just incremented
       * their reference counts, so unconditionally decrement their
       * reference counts without checking again.  If this crashes, it's
       * because the user misused the library, probably by decrementing
       * reference counts more than they were incremented.
       * */
      for (std::size_t j = 0; j < i; j++) {
        this->referenced_handles[j].lock()->decRef();
      }
      return;
    }
  }

  *out = this->event;
}

EventSender::EventSender() : pending_events(), producer_token(pending_events) {}

void EventSender::setEnabled(bool val) { this->enabled = val; }

bool EventSender::isEnabled() { return this->enabled; }

void EventSender::getNextEvent(syz_Event *out) {
  PendingEvent maybe_event;

  *out = syz_Event{};

  if (this->pending_events.try_dequeue(maybe_event) == false) {
    return;
  }

  maybe_event.extract(out);
}

void EventSender::enqueue(syz_Event &&event, EventHandleVec &&handles) {
  if (this->enabled == false) {
    return;
  }

  PendingEvent pending{std::move(event), std::move(handles)};
  this->pending_events.enqueue(this->producer_token, pending);
}

void EventBuilder::setSource(const std::shared_ptr<CExposable> &source) {
  /**
   * When lingering, sources can become nullptr because of shared_from_this
   * not having a non-NULL shared_ptr, as well as other now-read references.
   * Deal with that by remembering if that was the case, and not sending these
   * events.
   */
  if (this->associateObject(source)) {
    this->event.source = source->getCHandle();
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

void EventBuilder::setType(int type) { this->event.type = type; }

void EventBuilder::setPayload(const struct syz_UserAutomationEvent &payload) {
  this->event.payload.user_automation = payload;
  this->event.type = SYZ_EVENT_TYPE_USER_AUTOMATION;
}

void EventBuilder::dispatch(EventSender *sender) {
  if (this->will_send == false) {
    return;
  }
  if (this->has_source == false) {
    return;
  }

  assert(this->event.type != 0 && "Events must have a type");

  sender->enqueue(std::move(this->event), std::move(this->referenced_objects));
}

bool EventBuilder::associateObject(const std::shared_ptr<CExposable> &obj) {
  if (obj == nullptr || obj->isPermanentlyDead()) {
    return false;
  }
  auto weak = std::weak_ptr<CExposable>(obj);
  bool pushed = this->referenced_objects.pushBack(std::move(weak));
  // variable is flagged as unused, if the following assert isn't compiled in.
  (void)pushed;
  assert(pushed && "Event has too many referenced objects");
  return true;
}

void sendFinishedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source) {
  ctx->sendEvent([&](auto *builder) {
    builder->setSource(std::static_pointer_cast<CExposable>(source));
    builder->setContext(ctx);
    builder->setType(SYZ_EVENT_TYPE_FINISHED);
  });
}

void sendLoopedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source) {
  ctx->sendEvent([&](auto *builder) {
    builder->setSource(std::static_pointer_cast<CExposable>(source));
    builder->setContext(ctx);
    builder->setType(SYZ_EVENT_TYPE_LOOPED);
  });
}

void sendUserAutomationEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<BaseObject> &source,
                             unsigned long long param) {
  ctx->sendEvent([&](auto *builder) {
    builder->setSource(source);
    builder->setContext(ctx);
    builder->setType(SYZ_EVENT_TYPE_USER_AUTOMATION);
    builder->setPayload({param});
  });
}

} // namespace synthizer

SYZ_CAPI void syz_eventDeinit(struct syz_Event *event) {
  /*
   * Deinitialize an event by decrementing reference counts as necessary. We'll just
   * go through the C API and not check errors: syz_handleDecRef always succeeds save for programmer error.
   * */

  /* Invalid events are zero-initialized, so there's no need to check. */
  syz_handleDecRef(event->source);
  syz_handleDecRef(event->context);
}
