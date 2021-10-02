#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/automation_batch.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/context.hpp"
#include "synthizer/error.hpp"

#include <memory>
#include <utility>

namespace synthizer {

AutomationBatch::AutomationBatch(const std::shared_ptr<Context> &ctx)
    : context(ctx), context_validation_ptr(ctx.get()) {}

void AutomationBatch::automateProperty(const std::shared_ptr<BaseObject> &obj, int property,
                                       const PropertyAutomationPoint<6> &point) {
  this->throwIfConsumed();

  if (obj->getContextRaw() != this->context_validation_ptr) {
    throw EValidation("Object is from the wrong context");
  }

  obj->validateAutomation(property, &point);
  std::weak_ptr<BaseObject> obj_weak{obj};
  this->property_automation[std::move(obj_weak)][property].push_back(point);
}

void AutomationBatch::clearProperty(const std::shared_ptr<BaseObject> &obj, int property) {
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

void AutomationBatch::clearAllProperties(const std::shared_ptr<BaseObject> &obj) {
  std::weak_ptr<BaseObject> weak{obj};
  this->property_automation.erase(weak);
  this->cleared_properties.erase(weak);
  this->clear_all_properties.insert(weak);
}

void AutomationBatch::sendUserEvent(const std::shared_ptr<BaseObject> &obj, double time, unsigned long long param) {
  this->scheduled_events[std::weak_ptr<BaseObject>(obj)].emplace_back(std::make_tuple(time * config::SR, param));
}

void AutomationBatch::clearEvents(const std::shared_ptr<BaseObject> &obj) {
  std::weak_ptr<BaseObject> weak{obj};
  this->scheduled_events.erase(weak);
  this->clear_events.insert(weak);
}

void AutomationBatch::addCommands(std::size_t commands_len, const syz_AutomationCommand *commands) {
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

void AutomationBatch::executeOnContextThread() {
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

void AutomationBatch::consume() { this->consumed = true; }

void AutomationBatch::throwIfConsumed() {
  if (this->consumed) {
    throw ENotSupported("AutomationBatch cannot be reused after execution");
  }
}

} // namespace synthizer

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_createAutomationBatch(syz_Handle *out, syz_Handle context, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback) {
  SYZ_PROLOGUE
  auto ctx = fromC<Context>(context);
  auto batch = allocateSharedDeferred<AutomationBatch>(ctx);
  auto ce = std::static_pointer_cast<CExposable>(batch);
  ce->stashInternalReference(ce);
  *out = toC(batch);
  return syz_handleSetUserdata(*out, userdata, userdata_free_callback);
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_automationBatchAddCommands(syz_Handle batch, unsigned long long commands_len,
                                                      const struct syz_AutomationCommand *commands) {
  SYZ_PROLOGUE
  auto b = fromC<AutomationBatch>(batch);
  b->addCommands(commands_len, commands);
  return 0;
  SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_automationBatchExecute(syz_Handle batch) {
  SYZ_PROLOGUE
  auto b = fromC<AutomationBatch>(batch);
  b->throwIfConsumed();
  b->consume();
  auto c = b->getContext();
  if (c == nullptr) {
    return 0;
  }
  c->enqueueCallbackCommand([=]() { b->executeOnContextThread(); });

  return 0;
  SYZ_EPILOGUE
}
