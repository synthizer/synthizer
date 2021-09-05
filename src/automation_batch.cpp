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

} // namespace synthizer
