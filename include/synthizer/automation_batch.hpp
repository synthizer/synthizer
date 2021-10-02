#include "synthizer_constants.h"

#include "synthizer/memory.hpp"
#include "synthizer/property_automation_timeline.hpp"

#include <cstdint>
#include <memory>
#include <tuple>

struct syz_AutomationCommand;

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
class AutomationBatch : public CExposable {
public:
  AutomationBatch(const std::shared_ptr<Context> &ctx);

  int getObjectType() { return SYZ_OTYPE_AUTOMATION_BATCH; }
  /**
   * Append a property automation point to the batch.
   * */
  void automateProperty(const std::shared_ptr<BaseObject> &obj, int property, const PropertyAutomationPoint<6> &point);

  /**
   * Clear a property.
   * */
  void clearProperty(const std::shared_ptr<BaseObject> &obj, int property);

  void clearAllProperties(const std::shared_ptr<BaseObject> &obj);
  void sendUserEvent(const std::shared_ptr<BaseObject> &obj, double time, unsigned long long param);
  void clearEvents(const std::shared_ptr<BaseObject> &obj);

  /**
   * Should be called from the context thread only. Execute this batch.
   * */
  void executeOnContextThread();

  /**
   * Apply a batch of commands from the user.
   * */
  void addCommands(std::size_t commands_len, const struct syz_AutomationCommand *commands);

  /**
   * Lock this object so that it may never be executed again.
   * */
  void consume();

  /**
   * Throw an exception if consumed.
   * */
  void throwIfConsumed();

  /**
   * Get the context, returning nullptr if there isn't one.
   * */
  std::shared_ptr<Context> getContext() { return this->context.lock(); }

private:
  deferred_map<std::weak_ptr<BaseObject>, deferred_map<int, deferred_vector<PropertyAutomationPoint<6>>>,
               std::owner_less<std::weak_ptr<BaseObject>>>
      property_automation;
  deferred_map<std::weak_ptr<BaseObject>, deferred_vector<std::tuple<double, unsigned long long>>,
               std::owner_less<std::weak_ptr<BaseObject>>>
      scheduled_events;
  deferred_map<std::weak_ptr<BaseObject>, deferred_set<int>, std::owner_less<std::weak_ptr<BaseObject>>>
      cleared_properties;
  deferred_set<std::weak_ptr<BaseObject>, std::owner_less<std::weak_ptr<BaseObject>>> clear_all_properties;
  deferred_set<std::weak_ptr<BaseObject>, std::owner_less<std::weak_ptr<BaseObject>>> clear_events;

  std::weak_ptr<Context> context;
  // version of the context for faster validation.
  Context *context_validation_ptr = nullptr;
  bool consumed = false;
};

} // namespace synthizer