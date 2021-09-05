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

} // namespace synthizer