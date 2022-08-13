#include "synthizer_constants.h"

#include "synthizer/base_object.hpp"
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

inline AutomationBatch::AutomationBatch(const std::shared_ptr<Context> &ctx)
    : context(ctx), context_validation_ptr(ctx.get()) {}

inline void AutomationBatch::automateProperty(const std::shared_ptr<BaseObject> &obj, int property,
                                              const PropertyAutomationPoint<6> &point) {
  this->throwIfConsumed();

  if (obj->getContextRaw() != this->context_validation_ptr) {
    throw EValidation("Object is from the wrong context");
  }

  obj->validateAutomation(property, &point);
  std::weak_ptr<BaseObject> obj_weak{obj};
  this->property_automation[std::move(obj_weak)][property].push_back(point);
}

inline void AutomationBatch::clearProperty(const std::shared_ptr<BaseObject> &obj, int property) {
  this->throwIfConsumed();

  obj->validateAutomation(property, std::nullopt);

  std::weak_ptr<BaseObject> weak{obj};
  this->cleared_properties[weak].insert(property);
  auto found_obj = this->property_automation.find(weak);
  if (found_obj == this->property_automation.end()) {
    return;
  }
  auto found_vec = found_obj->second.find(property);
  if (found_vec == found_obj->second.end()) {
    return;
  }
  found_vec->second.clear();
}

inline void AutomationBatch::clearAllProperties(const std::shared_ptr<BaseObject> &obj) {
  std::weak_ptr<BaseObject> weak{obj};
  this->property_automation.erase(weak);
  this->cleared_properties.erase(weak);
  this->clear_all_properties.insert(weak);
}

inline void AutomationBatch::sendUserEvent(const std::shared_ptr<BaseObject> &obj, double time,
                                           unsigned long long param) {
  this->scheduled_events[std::weak_ptr<BaseObject>(obj)].emplace_back(std::make_tuple(time * config::SR, param));
}

inline void AutomationBatch::clearEvents(const std::shared_ptr<BaseObject> &obj) {
  std::weak_ptr<BaseObject> weak{obj};
  this->scheduled_events.erase(weak);
  this->clear_events.insert(weak);
}

inline void AutomationBatch::addCommands(std::size_t commands_len, const syz_AutomationCommand *commands) {
  this->throwIfConsumed();

  for (std::size_t i = 0; i < commands_len; i++) {
    const syz_AutomationCommand *cmd = commands + i;
    std::shared_ptr<BaseObject> obj = fromC<BaseObject>(cmd->target);

    switch (cmd->type) {
    case SYZ_AUTOMATION_COMMAND_APPEND_PROPERTY:
      this->automateProperty(obj, cmd->params.append_to_property.property,
                             PropertyAutomationPoint<6>(cmd->time * config::SR, &cmd->params.append_to_property.point));
      break;
    case SYZ_AUTOMATION_COMMAND_CLEAR_PROPERTY:
      this->clearProperty(obj, cmd->params.clear_property.property);
      break;
    case SYZ_AUTOMATION_COMMAND_CLEAR_ALL_PROPERTIES:
      this->clearAllProperties(obj);
      break;
    case SYZ_AUTOMATION_COMMAND_SEND_USER_EVENT:
      this->sendUserEvent(obj, cmd->time, cmd->params.send_user_event.param);
      break;
    case SYZ_AUTOMATION_COMMAND_CLEAR_EVENTS:
      this->clearEvents(obj);
      break;
    default:
      throw ENotSupported("This command isn't supported yet");
    }
  }
}

inline void AutomationBatch::executeOnContextThread() {
  auto ctx = this->context.lock();
  assert(ctx != nullptr && "Trying to execute code on the context's thread without the context running that thread");

  for (auto &obj : this->clear_all_properties) {
    auto strong = obj.lock();
    if (strong == nullptr) {
      continue;
    }
    strong->clearAllAutomation();
  }

  for (auto &obj : this->clear_events) {
    auto strong = obj.lock();
    if (!strong) {
      continue;
    }
    strong->automationClearScheduledEvents();
  }

  for (auto &[obj, props] : this->cleared_properties) {
    auto strong = obj.lock();
    if (strong == nullptr) {
      continue;
    }

    for (auto prop : props) {
      strong->clearAutomationForProperty(prop);
    }
  }

  for (auto &p : this->property_automation) {
    auto strong = p.first.lock();
    if (strong == nullptr) {
      continue;
    }

    for (auto &prop : p.second) {
      strong->applyPropertyAutomationPoints(prop.first, prop.second.size(), &prop.second[0]);
    }
  }

  for (auto &[obj, evts] : this->scheduled_events) {
    auto strong = obj.lock();
    if (!strong) {
      continue;
    }

    for (auto [time, param] : evts) {
      strong->automationScheduleEvent(time, param);
    }
  }
}

inline void AutomationBatch::consume() { this->consumed = true; }

inline void AutomationBatch::throwIfConsumed() {
  if (this->consumed) {
    throw ENotSupported("AutomationBatch cannot be reused after execution");
  }
}

} // namespace synthizer
