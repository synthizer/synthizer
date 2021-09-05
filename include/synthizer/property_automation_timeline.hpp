#pragma once

#include "synthizer.h"

#include "synthizer/memory.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace synthizer {

class PropertyAutomationPoint {
public:
  PropertyAutomationPoint(const struct syz_AutomationPoint *input);

  unsigned int interpolation_type;
  double automation_time;
  double value;
};

/**
 * A property automation timeline.
 *
 * This consists of a vector of points with their interpolation types, and a tick method.  Every time tick is called,
 * the new value is computed based off the points and the internal time of the timeline.  This is independent of the
 * context's view of time because that allows it to play nice with idle generators, pausing, and the like: users are
 * thus able to build and apply envelopes now or at some later time.
 *
 * From the external perspective, this is exposed via a `Timeline` object which can be applied to properties and reused.
 * */
class PropertyAutomationTimeline {
public:
  PropertyAutomationTimeline();

  /**
   * Tick the timeline, updating the internally stored next value.
   *
   * Needs to be called *before* each audio tick.
   *
   * Time is in seconds and must be monatonically increasing.
   * */
  void tick(double time);

  /**
   * Add a point to this timeline.
   * */
  void addPoint(const PropertyAutomationPoint &point);
  /**
   * Get the value of the timeline at the current time.
   *
   * Usage is like:
   *
   * - Somewhere calls `tick()` for the start of this audio tick
   * - Somewhere else calls `getValue()`.
   *
   * This function returns an empty option if the timeline hasn't started yet (e.g. is in the future) or has
   * already ended.  When it does so, properties are left alone.
   * */
  std::optional<double> getValue() { return this->current_value; }

  /**
   * Returns true when evaluating this timeline will never again produce a value.
   * */
  bool isFinished() { return this->finished; }

  void clear();

private:
  void resortIfNeeded();

  deferred_vector<PropertyAutomationPoint> points;
  /**
   * Points at the next point which we may need to evaluate.
   * */
  std::size_t next_point = 0;
  bool finished = true;
  bool has_added_since_last_sort = true;
  std::optional<double> current_value = std::nullopt;
};

/**
 * The C exposed version of the AutomationTimeline, which contains a set of user-specified points and allows for
 * applying them to a property repeatedly by cloning the internally maintained timeline.
 * */
class ExposedAutomationTimeline : public CExposable {
public:
  ExposedAutomationTimeline(std::size_t points_len, const struct syz_AutomationPoint *input_points);

  std::shared_ptr<PropertyAutomationTimeline> buildTimeline();
  int getObjectType() { return SYZ_OTYPE_AUTOMATION_TIMELINE; }

private:
  std::vector<PropertyAutomationPoint> points;
};

} // namespace synthizer