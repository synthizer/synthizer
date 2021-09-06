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
    : interpolation_type(input->interpolation_type),
      automation_time(input->automation_time), values{input->values[0], input->values[1], input->values[2],
                                                      input->values[3], input->values[4], input->values[5]} {}

PropertyAutomationTimeline::PropertyAutomationTimeline() { this->points.reserve(1024); }

void PropertyAutomationTimeline::resortIfNeeded() {
  if (this->has_added_since_last_sort) {
    pdqsort_branchless(this->points.begin(), this->points.end(),
                       [](auto &a, auto &b) { return a.automation_time < b.automation_time; });
    this->has_added_since_last_sort = false;
  }
}

void PropertyAutomationTimeline::addPoint(const PropertyAutomationPoint &point) {
  this->points.push_back(point);
  this->has_added_since_last_sort = true;
  this->finished = false;
}

void PropertyAutomationTimeline::clear() {
  this->points.clear();
  this->finished = true;
}

} // namespace synthizer
