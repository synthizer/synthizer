#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/memory.hpp"

#include <pdqsort.h>

#include <algorithm>
#include <array>
#include <cassert>
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
  std::array<double, 6> values;
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
   *
   * The template parameter controls how many points to evaluate, and is usually set to 1, 3, or 6.  This is used to
   * differentiate between which kind of property the timeline is for.
   * */
  template <unsigned int N> void tick(double time);

  /**
   * Add a point to this timeline.
   * */
  void addPoint(const PropertyAutomationPoint &point);

  template <typename IT> void addPoints(IT begin, IT end) {
    for (auto i = begin; i < end; i++) {
      addPoint(*i);
    }
  }

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
  std::optional<std::array<double, 6>> getValue() { return this->current_value; }

  /**
   * Returns true when evaluating this timeline will never again produce a value.
   * */
  bool isFinished() { return this->finished; }

  void clear();

  /**
   * When we're this many points into the timeline, get rid of all the ones at the beginning that we no longer need.
   * */
  std::size_t COPY_BACK_THRESHOLD = 128;
private:
  void resortIfNeeded();

  deferred_vector<PropertyAutomationPoint> points;
  /**
   * Points at the next point which we may need to evaluate.
   * */
  std::size_t next_point = 0;
  bool finished = true;
  bool has_added_since_last_sort = true;
  std::optional<std::array<double, 6>> current_value = std::nullopt;
};

template <unsigned int N> inline void PropertyAutomationTimeline::tick(double time) {
  static_assert(N > 0 && N <= 6, "N is out of range");

  this->resortIfNeeded();

  if (this->finished) {
    this->current_value = std::nullopt;
    return;
  }

  // Advance until we find a point to evaluate.
  // We could binary search but it's almost always going to be the next point.
  while (this->next_point < this->points.size() && this->points[this->next_point].automation_time <= time) {
    this->next_point++;
  }
  if (this->next_point >= this->points.size()) {
    // We'll become nullopt on the next time through, but we always want to make sure the last value of the
    // timeline is hit so that things always end up on a known state.
    this->current_value = this->points.back().values;
    this->finished = true;
    return;
  }

  // If we're exactly at the first point, start there; this is the common use case of specifying timelines that start
  // at 0.
  if (this->points.front().automation_time == time) {
    this->current_value = this->points.front().values;
  }

  // If we're not past the first point yet, nothing to do.
  if (this->next_point == 0) {
    this->current_value = std::nullopt;
    return;
  }

  std::size_t last_point = this->next_point - 1;

  const PropertyAutomationPoint &p1 = this->points[last_point];
  const PropertyAutomationPoint &p2 = this->points[next_point];

  // If the previous point's interpolation type is none, then we may not have jumped yet.  We can just unconditionally
  // do that here, since jumping to the same value twice is not a big deal.
  //
  // If the next point is NONE, then we must also jump: we need to finish the previous linear interpolation because
  // the value between a linear point and a none point is the value of the linear point.
  //
  // We don't return here: if the prior point is NONE then we might be interpolating.
  if (p1.interpolation_type == SYZ_INTERPOLATION_TYPE_NONE || p2.interpolation_type == SYZ_INTERPOLATION_TYPE_NONE) {
    this->current_value = p1.values;
  }

  // If p2 is NONE, we don't do anything with it until we cross it.
  if (p2.interpolation_type != SYZ_INTERPOLATION_TYPE_NONE) {
    double time_diff = p2.automation_time - p1.automation_time;
    double delta = (time - p1.automation_time) / time_diff;
    double w2 = delta;
    double w1 = 1.0 - w2;
    std::array<double, 6> value;
    for (unsigned int i = 0; i < N; i++) {
      value[i] = w1 * p1.values[i] + w2 * p2.values[i];
    }
    this->current_value = value;
  }

  // If last_point isn't 0, then we have points before last_point which we no longer need; roll back the future so that
  // the timeline doesn't fill indefinitely.
  //
  // last_point is always 1 before the end, because otherwise we'd not have a next_point and/or the if statements above
  // would have bailed.
  if (last_point > PropertyAutomationTimeline::COPY_BACK_THRESHOLD) {
    auto needed_begin = this->points.begin() + last_point;
    std::copy(needed_begin, this->points.end(), this->points.begin());
    // last_point is at the beginning, next_point is 1 after that.  Fix the next_point to be right.
    this->next_point = 1;
  }
}

} // namespace synthizer
