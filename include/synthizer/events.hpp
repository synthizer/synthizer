#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/memory.hpp"
#include "synthizer/small_vec.hpp"

#include <concurrentqueue.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

namespace synthizer {

class CExposable;

/**
 * Event system internals.  See the comments on each of these classes, which explain how everything works.
 * */

const std::size_t EVENT_HANDLE_CAPACITY = 4;
using EventHandleVec = SmallVec<std::weak_ptr<CExposable>, EVENT_HANDLE_CAPACITY>;

/**
 * An event which is ready to be fired.
 *
 * Events only actually fire if all of the strongly-referenced handles are still alive. This allows automatically
 * dropping any pending events that refer to deleted objects without having to deal with having individual per-object
 * queues, which would have significant overhead.
 * */
class PendingEvent {
public:
  /**
   * moodycamel::ConcurrentQueue needs to have objects to enqueue into, so this must be default constructable.
   * We record that it's not valid, and assert in extract since this being invalid is a logic bug.
   * */
  PendingEvent();

  /**
   * For safety/correctness purposes, make it such that it's only possible to construct this
   * by moving the components.
   * */
  PendingEvent(syz_Event &&event, EventHandleVec &&referenced_handles);

  /**
   * The method called by the C API: writes the payload to the out pointer.
   * The out pointer will be zero-initialized if the event didn't fire, which sets the event type to
   * SYZ_EVENT_TYPE_INVALID.
   * */
  void extract(syz_Event *out);

  void enqueue(syz_Event &&event, EventHandleVec &&handles);

private:
  syz_Event event;
  EventHandleVec referenced_handles;
  bool valid = false;
};

/**
 * The EventSender just wraps a queue and whether or not it's enabled.
 *
 * Starts disabled.
 *
 * Events should only be sent from one thread, though consuming from multiple threads is fine.
 * */
class EventSender {
public:
  EventSender();

  void setEnabled(bool val);
  bool isEnabled();

  /**
   * Get an event from the queue and fire it if possible.
   *
   * Called by syz_contextGetNextEvent.
   * */
  void getNextEvent(syz_Event *out);

  void enqueue(syz_Event &&event, EventHandleVec &&handles);

private:
  moodycamel::ConcurrentQueue<PendingEvent> pending_events;
  moodycamel::ConcurrentQueue<PendingEvent>::producer_token_t producer_token;
  bool enabled = false;
};

inline PendingEvent::PendingEvent() {}

inline PendingEvent::PendingEvent(syz_Event &&event, EventHandleVec &&referenced_handles)
    : event(event), referenced_handles(referenced_handles), valid(true) {}

inline void PendingEvent::extract(syz_Event *out) {
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

inline EventSender::EventSender() : pending_events(), producer_token(pending_events) {}

inline void EventSender::setEnabled(bool val) { this->enabled = val; }

inline bool EventSender::isEnabled() { return this->enabled; }

inline void EventSender::getNextEvent(syz_Event *out) {
  PendingEvent maybe_event;

  *out = syz_Event{};

  if (this->pending_events.try_dequeue(maybe_event) == false) {
    return;
  }

  maybe_event.extract(out);
}

inline void EventSender::enqueue(syz_Event &&event, EventHandleVec &&handles) {
  if (this->enabled == false) {
    return;
  }

  PendingEvent pending{std::move(event), std::move(handles)};
  this->pending_events.enqueue(this->producer_token, pending);
}

} // namespace synthizer
