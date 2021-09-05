#include "synthizer/property_internals.hpp"

#include "synthizer/base_object.hpp"

namespace synthizer {

void setPropertyCmd(int property, std::weak_ptr<BaseObject> target_weak, property_impl::PropertyValue value) {
  auto target = target_weak.lock();
  if (target == nullptr) {
    return;
  }

  target->setProperty(property, value);
}

void automatePropertyCmd(int property, std::weak_ptr<BaseObject> target_weak,
                         std::shared_ptr<ExposedAutomationTimeline> timeline) {
  auto strong = target_weak.lock();
  if (strong == nullptr) {
    return;
  }
  strong->automateProperty(property, timeline);
}

} // namespace synthizer