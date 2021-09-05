#include "synthizer/automation_batch.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/context.hpp"
#include "synthizer/error.hpp"

#include <memory>
#include <utility>

namespace synthizer {

AutomationBatch::AutomationBatch(const std::shared_ptr<Context> &ctx)
    : context(ctx), context_validation_ptr(ctx.get()) {}

void AutomationBatch::automateProperty(const std::shared_ptr<BaseObject> &obj, int property,
                                       const PropertyAutomationPoint &point) {
  if (obj->getContextRaw() != this->context_validation_ptr) {
    throw EValidation("Object is from the wrong context");
  }

  obj->validateAutomation(property);
  std::weak_ptr<BaseObject> obj_weak{obj};
  this->property_automation[std::move(obj_weak)][property].push_back(point);
}

void AutomationBatch::execute() {
  auto ctx = this->context.lock();
  assert(ctx != nullptr && "Trying to execute code on the context's thread without the context running that thread");

  for (auto &p : this->property_automation) {
    auto strong = p.first.lock();
    if (strong == nullptr) {
      continue;
    }

    for (auto &prop : p.second) {
      strong->applyPropertyAutomationPoints(prop.first, prop.second.size(), &prop.second[0]);
    }
  }
}

} // namespace synthizer

