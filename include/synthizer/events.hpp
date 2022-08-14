#pragma once

#include "synthizer/small_vec.hpp"

#include <concurrentqueue.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace synthizer {

class CExposable;
class Context;

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

} // namespace synthizer
