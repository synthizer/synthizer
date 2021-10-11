#pragma once
#include "synthizer.h"

#include "synthizer/error.hpp"
#include "synthizer/event_timeline.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/property_automation_timeline.hpp"
#include "synthizer/property_internals.hpp"

#include <atomic>
#include <cassert>
#include <memory>
#include <optional>

namespace synthizer {

/* Forward declare the routing handles. */
namespace router {
class InputHandle;
class OutputHandle;
} // namespace router

class Context;

/*
 * The ultimate base class for all Synthizer objects except those which aren't associated with a context.
 *
 * To use this properly, make sure that you virtual inherit from this directly or indirectly if adding a new object
 * type.
 *
 * Synthizer does initialization as a two-step process:
 * - First the constructor is called. It should not rely on being
 *   in the audio thread, and may be called from any thread.
 * - Then initInAudioThread is called, which is in the context's audio thread. initInAudioThread should try to avoid
 * allocating if at all possible.
 *
 * IMPORTANT: initInAudioThread is called async, and may be called after other calls to the object via the C API. This
 * is because blocking on the audio thread introduces a huge amount of latency and performance degradation.
 * */
class BaseObject : public CExposable {
public:
  /*
   * Constructors of derived objects should only allocate, and should otherwise be threadsafe.
   * */
  BaseObject(const std::shared_ptr<Context> &ctx);

  /*
   * This is called in the audio thread to initialize the object and should try not to allocate.
   * */
  virtual void initInAudioThread();

  virtual ~BaseObject();

  /* A baseObject has no properties whatsoever. */
  virtual bool hasProperty(int property);

  virtual property_impl::PropertyValue getProperty(int property);

  virtual void validateProperty(int property, const property_impl::PropertyValue &value);

  virtual void setProperty(int property, const property_impl::PropertyValue &value);

  /**
   * If no point is provided, only validate that we can automate the property. Lets' the function also be usedfor
   * clearing.
   * */
  virtual void validateAutomation(int property, std::optional<const PropertyAutomationPoint<6> *> point);

  /**
   * Unreachable. Apply a set of automation points to an object.
   * */
  virtual void applyPropertyAutomationPoints(int property, std::size_t points_len, PropertyAutomationPoint<6> *points);

  /**
   * Should also be called only after AutomateProperty. Is unreachable; we need this for the property infrastructure to
   * always have a base class.
   * */
  virtual void clearAutomationForProperty(int property);

  virtual void clearAllPropertyAutomation();
  void clearAllAutomation();

  /* Virtual because context itself needs to override to always return itself. */
  virtual std::shared_ptr<Context> getContext();

  /* From the C API implementations, you usually want this one. */
  virtual Context *getContextRaw();

  /*
   * Routing. If either of these returns an actual non-nullptr value, this object supports that half
   * of the routing architecture (i.e. readers are effects, writers are sources).
   * */
  virtual router::InputHandle *getInputHandle();
  virtual router::OutputHandle *getOutputHandle();

  void signalLingerStopPoint() override;

  /**
   * Advance automation.  This internal method is for the property infrastructure, and ticks any associated timelines.
   * Should be called by tickAutomation, which can be overridden by subclasses to e.g. not run automation only once
   *
   * Only call once per generated block (e.g. it makes sense to call this if pitch bend has you doing two blocks for
   * one).
   * */
  virtual void propSubsystemAdvanceAutomation();

  /**
   * Tick property automation.  The default implementation just calls propSubsystemAdvanceAutomation, but subclasses
   * sometimes opt to short-circuit and not advance it at all (e.g. Pasuable).
   * */
  virtual void tickAutomation();

  /**
   * Get the local automation time, which updates every time automation specifically for this object ticks.
   *
   * */
  double getAutomationTimeInSamples();
  unsigned int getAutomationTimeInBlocks();

  /**
   * Schedule an event at a specific time, which will be dispatched to the user.
   * */
  void automationScheduleEvent(double time, unsigned long long param);

  void automationClearScheduledEvents();

protected:
  std::shared_ptr<Context> context;
  /* incremented every block. Read by the user on non-audio threads when asking for time properties. */
  std::atomic<unsigned int> local_block_time = 0;
  EventTimeline scheduled_events;
};

} // namespace synthizer