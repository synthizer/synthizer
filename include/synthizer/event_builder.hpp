#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/context.hpp"
#include "synthizer/events.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/small_vec.hpp"

#include <concurrentqueue.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

namespace synthizer {
/**
 * The EventBuilder is a safe abstraction for building events, or at least
 * as safe as C++ allows.  To use it:
 *
 * - call setSource to set the source.
 * - Maybe call setContext to set the context.
 * - call translateHandle for every object that needs to be a handle in the event struct. This may return
 *   0 if the object doesn't have a handle, but will short-circuit event sending in that case.
 * - Build one of the payload event structs.
 * - call setPayload with the payload struct, which will handle
 *   setting up the union via overloads.
 * - call dispatch with a pointer to an EventSender to send the event.
 *
 * For convenience, translateHandle has a weak_ptr overload.
 * */
class EventBuilder {
public:
  void setSource(const std::shared_ptr<CExposable> &source);
  void setContext(const std::shared_ptr<Context> &ctx);

  syz_Handle translateHandle(const std::shared_ptr<CExposable> &object);
  syz_Handle translateHandle(const std::weak_ptr<CExposable> &object);
  void setType(int type);

  /* These overloads set the payload, and the type that goes with that payload when the type is known. */

  void setPayload(const syz_UserAutomationEvent &payload);

  void dispatch(EventSender *sender);

private:
  /**
   * Associate a handle with this event, return whether or not the object was valid (e.g. non-null, still visible from
   * C)
   * */
  bool associateObject(const std::shared_ptr<CExposable> &obj);

  syz_Event event{};
  EventHandleVec referenced_objects;
  bool will_send = true;
  bool has_source = false;
};

/**
 * senders for a couple common cases.
 * */
void sendFinishedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source);
void sendLoopedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source);
void sendUserAutomationEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<BaseObject> &source,
                             unsigned long long param);

inline void EventBuilder::setSource(const std::shared_ptr<CExposable> &source) {
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

inline void EventBuilder::setContext(const std::shared_ptr<Context> &ctx) {
  auto base = std::static_pointer_cast<CExposable>(ctx);
  this->event.context = this->translateHandle(base);
}

inline syz_Handle EventBuilder::translateHandle(const std::shared_ptr<CExposable> &object) {
  if (this->associateObject(object) == false) {
    this->will_send = false;
    return 0;
  }
  return object->getCHandle();
}

inline syz_Handle EventBuilder::translateHandle(const std::weak_ptr<CExposable> &object) {
  if (object.expired() == true) {
    this->will_send = false;
    return 0;
  }
  return this->translateHandle(object.lock());
}

inline void EventBuilder::setType(int type) { this->event.type = type; }

inline void EventBuilder::setPayload(const struct syz_UserAutomationEvent &payload) {
  this->event.payload.user_automation = payload;
  this->event.type = SYZ_EVENT_TYPE_USER_AUTOMATION;
}

inline void EventBuilder::dispatch(EventSender *sender) {
  if (this->will_send == false) {
    return;
  }
  if (this->has_source == false) {
    return;
  }

  assert(this->event.type != 0 && "Events must have a type");

  sender->enqueue(std::move(this->event), std::move(this->referenced_objects));
}

inline bool EventBuilder::associateObject(const std::shared_ptr<CExposable> &obj) {
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

inline void sendFinishedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source) {
  ctx->sendEvent([&](auto *builder) {
    builder->setSource(std::static_pointer_cast<CExposable>(source));
    builder->setContext(ctx);
    builder->setType(SYZ_EVENT_TYPE_FINISHED);
  });
}

inline void sendLoopedEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<CExposable> &source) {
  ctx->sendEvent([&](auto *builder) {
    builder->setSource(std::static_pointer_cast<CExposable>(source));
    builder->setContext(ctx);
    builder->setType(SYZ_EVENT_TYPE_LOOPED);
  });
}

inline void sendUserAutomationEvent(const std::shared_ptr<Context> &ctx, const std::shared_ptr<BaseObject> &source,
                                    unsigned long long param) {
  ctx->sendEvent([&](auto *builder) {
    builder->setSource(source);
    builder->setContext(ctx);
    builder->setType(SYZ_EVENT_TYPE_USER_AUTOMATION);
    builder->setPayload({param});
  });
}

} // namespace synthizer
