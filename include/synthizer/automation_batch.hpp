#include "synthizer/memory.hpp"
#include "synthizer/property_automation_timeline.hpp"

#include <memory>

namespace synthizer {

class baseObject;
class Context;

/**
 * Represents a group of automation events which will be applied to a set of objects as a batch.
 *
 * This exists to allow users of Synthizer to atomically set automation across the entire property graph.
 * */
class AutomationBatch : CExposable {
public:
  AutomationBatch(const std::shared_ptr<Context> &ctx);

  /**
   * Append a property automation point to the batch, validating that it can be applied to the specified object by
   * throwing an exception when this is not the case.
   * */
  void automateProperty(const std::shared_ptr<BaseObject> &obj, int property, const PropertyAutomationPoint &point);

private:
  deferred_map<std::weak_ptr<BaseObject>, deferred_map<int, deferred_vector<PropertyAutomationPoint>>,
               std::owner_less<std::weak_ptr<BaseObject>>>
      property_automation;
  std::weak_ptr<Context> context;
  // version of the context for faster validation.
  Context *context_validation_ptr = nullptr;
};

} // namespace synthizer