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
                                       const PropertyAutomationPoint &point) {
  this->throwIfConsumed();

  if (obj->getContextRaw() != this->context_validation_ptr) {
    throw EValidation("Object is from the wrong context");
  }

  obj->validateAutomation(property);
  std::weak_ptr<BaseObject> obj_weak{obj};
  this->property_automation[std::move(obj_weak)][property].push_back(point);
}

void AutomationBatch::clearProperty(const std::shared_ptr<BaseObject> &obj, int property) {
  this->throwIfConsumed();

  obj->validateAutomation(property);

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

void AutomationBatch::addCommands(std::size_t commands_len, const struct syz_AutomationBatchCommand *commands) {
  this->throwIfConsumed();

  (void)commands_len;
  (void)commands;

  throw ENotSupported("This command isn't supported yet");
}

void AutomationBatch::executeOnContextThread() {
  auto ctx = this->context.lock();
  assert(ctx != nullptr && "Trying to execute code on the context's thread without the context running that thread");

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
}

void AutomationBatch::consume() { this->consumed = true; }

void AutomationBatch::throwIfConsumed() { throw ENotSupported("AutomationBatch cannot be reused after execution"); }

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
                                                      const struct syz_AutomationBatchCommand *commands) {
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
