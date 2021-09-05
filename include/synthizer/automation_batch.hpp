#include "synthizer/memory.hpp"
#include "synthizer/property_automation_timeline.hpp"

#include <memory>

namespace synthizer {

class BaseObject;
class Context;

/**
 * Represents a group of automation events which will be applied to a set of objects as a batch.
 *
 * This exists to allow users of Synthizer to atomically set automation across the entire property graph.
 *
 * All functions except execute must be called outside the audio thread and throw exceptions on validation errors;
 * execute then applies the batch in the audio thread (put another way, everything but execute is called by the C API).
 * */
class AutomationBatch : CExposable {
public:
  AutomationBatch(const std::shared_ptr<Context> &ctx);

  /**
   * Append a property automation point to the batch.
   * */
  void automateProperty(const std::shared_ptr<BaseObject> &obj, int property, const PropertyAutomationPoint &point);

  void clearProperty(const std::shared_ptr<BaseObject> &obj, int property);

  /**
   * Clear a property.

  /**
   * Should be called from the context thread only. Execute this batch.
   * */
  void execute();

private:
  deferred_map<std::weak_ptr<BaseObject>, deferred_map<int, deferred_vector<PropertyAutomationPoint>>,
               std::owner_less<std::weak_ptr<BaseObject>>>
      property_automation;
  deferred_map<std::weak_ptr<BaseObject>, deferred_set<int>, std::owner_less<std::weak_ptr<BaseObject>>>
      cleared_properties;
  std::weak_ptr<Context> context;
  // version of the context for faster validation.
  Context *context_validation_ptr = nullptr;
};

} // namespace synthizer