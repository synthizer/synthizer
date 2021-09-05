#include "synthizer_constants.h"

#include "synthizer/property_automation_timeline.hpp"

#include "synthizer/base_object.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/error.hpp"
#include "synthizer/memory.hpp"

#include <pdqsort.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace synthizer {

PropertyAutomationPoint::PropertyAutomationPoint(const struct syz_AutomationPoint *input)
    : interpolation_type(input->interpolation_type), automation_time(input->automation_time), value(input->values[0]) {}

PropertyAutomationTimeline::PropertyAutomationTimeline(const std::vector<PropertyAutomationPoint> &_points) {
  if (_points.size() == 0) {
    throw Error("Automation timelines may not have 0 points");
  }
  // We have to manually copy across because of the mismatch in the allocators.
  this->points = deferred_vector<PropertyAutomationPoint>(_points.begin(), _points.end());
}

void PropertyAutomationTimeline::tick(double time) {
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
    this->current_value = this->points.back().value;
    this->finished = true;
    return;
  }

  // If we're exactly at the first point, start there; this is the common use case of specifying timelines that start
  // at 0.
  if (this->points.front().automation_time == time) {
    this->current_value = this->points.front().value;
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
    this->current_value = p1.value;
  }

  // If p2 is NONE, we don't do anything with it until we cross it.
  if (p2.interpolation_type == SYZ_INTERPOLATION_TYPE_NONE) {
    return;
  }

  // Otherwise, we're fading to it.
  double time_diff = p2.automation_time - p1.automation_time;
  double delta = (time - p1.automation_time) / time_diff;
  double w2 = delta;
  double w1 = 1.0 - w2;
  double value = w1 * p1.value + w2 * p2.value;
  this->current_value = value;
}

ExposedAutomationTimeline::ExposedAutomationTimeline(std::size_t points_len,
                                                     const struct syz_AutomationPoint *input_points) {
  if (points_len == 0) {
    throw EValidation("Timelines must have at least 1 point");
  }

  for (std::size_t i = 0; i < points_len; i++) {
    this->points.emplace_back(PropertyAutomationPoint(&input_points[i]));
  }

  pdqsort_branchless(this->points.begin(), this->points.end(),
                     [](const auto &a, const auto &b) { return a.automation_time < b.automation_time; });
}

std::shared_ptr<PropertyAutomationTimeline> ExposedAutomationTimeline::buildTimeline() {
  return allocateSharedDeferred<PropertyAutomationTimeline>(this->points);
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createAutomationTimeline(syz_Handle *out, unsigned int point_count,
                                                    const struct syz_AutomationPoint *points, unsigned long long flags,
                                                    void *userdata, syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  if (flags != 0) {
    throw EValidation("flags is reserved");
  }

  auto x = allocateSharedDeferred<ExposedAutomationTimeline>(point_count, points);
  auto c = std::static_pointer_cast<CExposable>(x);
  c->stashInternalReference(c);
  *out = toC(c);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}
