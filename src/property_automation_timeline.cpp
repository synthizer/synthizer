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

void PropertyAutomationTimeline::addPoint(const PropertyAutomationPoint &point) { this->inner.appendItem(point); }

void PropertyAutomationTimeline::clear() { this->inner.clear(); }

} // namespace synthizer
