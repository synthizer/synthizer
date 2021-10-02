#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/generic_timeline.hpp"
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

template <std::size_t N> class PropertyAutomationPoint {
public:
  static_assert(N == 1 || N == 3 || N == 6);

  PropertyAutomationPoint(double time, const struct syz_AutomationPoint *input);
  /**
   * In order to use virtual functions, we have to start with PropertyAutomationTimeline<6> everywhere, then convert
   * down as needed.
   * */
  PropertyAutomationPoint(const PropertyAutomationPoint<6> &other);
  PropertyAutomationPoint(double _time, unsigned int _interpolation_type, const std::array<double, N> &_values);

  unsigned int interpolation_type;
  double automation_time;
  std::array<double, N> values;

  /* For GenericTimeline. */
  double getTime() { return this->automation_time; }
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
template <std::size_t N> class PropertyAutomationTimeline {
public:
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
  void tick(double time);

  /**
   * Add a point to this timeline.
   * */
  void addPoint(const PropertyAutomationPoint<N> &point);

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
   * This function returns an empty option if the timeline hasn't started yet (e.g. is in the future).  We rely on
   * properties optimizing their writes and always return the ending value once past the end of the timeline.
   * */
  std::optional<std::array<double, N>> getValue() { return this->current_value; }

  void clear();

private:
  GenericTimeline<PropertyAutomationPoint<N>, 1> inner;
  std::optional<std::array<double, N>> current_value = std::nullopt;
};

template <std::size_t N>
inline PropertyAutomationPoint<N>::PropertyAutomationPoint(double time, const struct syz_AutomationPoint *input)
    : interpolation_type(input->interpolation_type), automation_time(time) {
  for (std::size_t i = 0; i < N; i++)
    this->values[i] = input->values[i];
}

template <std::size_t N>
inline PropertyAutomationPoint<N>::PropertyAutomationPoint(const PropertyAutomationPoint<6> &other)
    : interpolation_type(other.interpolation_type), automation_time(other.automation_time) {
  for (std::size_t i = 0; i < N; i++) {
    this->values[i] = other.values[i];
  }
}

template <std::size_t N>
PropertyAutomationPoint<N>::PropertyAutomationPoint(double _time, unsigned int _interpolation_type,
                                                    const std::array<double, N> &_values)
    : automation_time(_time), interpolation_type(_interpolation_type), values(_values) {}

template <std::size_t N> inline void PropertyAutomationTimeline<N>::addPoint(const PropertyAutomationPoint<N> &point) {
  this->inner.addItem(point);
}

template <std::size_t N> inline void PropertyAutomationTimeline<N>::clear() {
  this->inner.clear();
  this->current_value = std::nullopt;
}

template <std::size_t N> inline void PropertyAutomationTimeline<N>::tick(double time) {
  this->inner.tick(time);

  auto maybe_cur = this->inner.getItem(0);
  if (!maybe_cur) {
    // Timeline is empty.
    this->current_value = std::nullopt;
    return;
  }
  auto cur = *maybe_cur;

  auto maybe_last = this->inner.getItem(-1);

  // If we're ahead of cur, we're done with the timeline; apply and hold the value.
  if (cur->automation_time <= time) {
    this->current_value = cur->values;
    return;
  }

  // If there is no last point, then the timeline hasn't started or was cleared.
  // Don't do anything; we'll pick up new values next time someone sets something.
  if (!maybe_last) {
    this->current_value = std::nullopt;
    return;
  }

  auto last = *maybe_last;

  // If the previous point's interpolation type is none, then we may not have jumped yet.  We can just unconditionally
  // do that here, since jumping to the same value twice is not a big deal.
  //
  // If the next point is NONE, then we must also jump to the value of the previous point: we need to finish the
  // previous linear interpolation because the value between a linear point and a none point is the value of the linear
  // point.
  //
  // We don't return here: if the prior point is NONE then we might be interpolating.
  if (last->interpolation_type == SYZ_INTERPOLATION_TYPE_NONE ||
      cur->interpolation_type == SYZ_INTERPOLATION_TYPE_NONE) {
    this->current_value = last->values;
  }

  // If cur is NONE, we don't do anything with it until we cross it.
  if (cur->interpolation_type != SYZ_INTERPOLATION_TYPE_NONE) {
    double time_diff = cur->automation_time - last->automation_time;
    if (time_diff <= 0) {
      // Possible due to either rounding error or overlapping points. Just use the latter point.
      this->current_value = cur->values;
      return;
    }

    // We're between cur and last, do an interpolation.
    double delta = (time - last->automation_time) / time_diff;
    double w2 = delta;
    double w1 = 1.0 - w2;
    std::array<double, N> value;
    for (unsigned int i = 0; i < N; i++) {
      value[i] = w1 * last->values[i] + w2 * cur->values[i];
    }
    this->current_value = value;
  }
}

} // namespace synthizer
