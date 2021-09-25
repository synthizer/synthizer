#pragma once

#include "synthizer/generic_timeline.hpp"

#include <memory>

namespace synthizer {

class BaseObject;
class Context;

class ScheduledEvent {
public:
  ScheduledEvent(double _time, unsigned long long _param) : time(_time), param(_param) {}
  double getTime() { return this->time; }

  double time;
  unsigned long long param;
};

/**
 * A timeline of scheduled events, which will auto-dispatch them against the context.
 *
 * Like with PropertyAutomationTimeline, this must have a monatomically increasing time parameter passed to the tick
 * method.  This timeline's tick also takes a context shared_ptr to use to dispatch events against.
 * */
class EventTimeline {
public:
  EventTimeline() {}

  void tick(const std::shared_ptr<Context> &ctx, const std::shared_ptr<BaseObject> &source, double time);

  void addItem(const ScheduledEvent &e);
  void clear();

private:
  GenericTimeline<ScheduledEvent, 1> timeline;
};

} // namespace synthizer