#include "synthizer/base_object.hpp"
#include "synthizer/context.hpp"
#include "synthizer/event_timeline.hpp"

#include <memory>

namespace synthizer {

BaseObject::BaseObject(const std::shared_ptr<Context> &ctx) : context(ctx) {}

void BaseObject::initInAudioThread() {}

BaseObject::~BaseObject() {}

bool BaseObject::hasProperty(int property) {
  (void)property;

  return false;
}

property_impl::PropertyValue BaseObject::getProperty(int property) {
  (void)property;

  switch (property) {
  case SYZ_P_CURRENT_TIME:
    return this->getAutomationTimeInSamples() / config::SR;
  // Add around 100ms to the current time. We might do a more complex algorithm later.
  case SYZ_P_SUGGESTED_AUTOMATION_TIME:
    return this->getAutomationTimeInSamples() / config::SR + 0.1;
  default:
    throw EInvalidProperty();
  }
}

void BaseObject::validateProperty(int property, const property_impl::PropertyValue &value) {
  (void)property;
  (void)value;

  switch (property) {
  case SYZ_P_CURRENT_TIME:
    throw EValidation("SYZ_P_CURRENT_TIME cannot be set");
  case SYZ_P_SUGGESTED_AUTOMATION_TIME:
    throw EValidation("SYZ_P_SUGGESTED_AUTOMATION_TIME cannot be set");
  default:
    // Don't give a special message.
    throw EInvalidProperty();
  }
}

void BaseObject::setProperty(int property, const property_impl::PropertyValue &value) {
  (void)property;
  (void)value;

  throw EInvalidProperty();
}

void BaseObject::validateAutomation(int property, std::optional<const PropertyAutomationPoint<6> *> point) {
  (void)point;

  // if we got here, we either don't have the properety or don't support automation.  But these lead to different
  // errors.
  if (this->hasProperty(property)) {
    throw ENotSupported("This property doesn't support automation");
  } else {
    throw EInvalidProperty();
  }
}

void BaseObject::applyPropertyAutomationPoints(int property, std::size_t points_len,
                                               PropertyAutomationPoint<6> *points) {
  (void)property;
  (void)points_len;
  (void)points;

  assert(false && "Unreachable!");
}

void BaseObject::clearAutomationForProperty(int property) {
  (void)property;
  assert(false && "Unreachable");
}

void BaseObject::clearAllPropertyAutomation() {}
void BaseObject::clearAllAutomation() { this->clearAllPropertyAutomation(); }

std::shared_ptr<Context> BaseObject::getContext() { return this->context; }
Context *BaseObject::getContextRaw() { return this->context.get(); }
router::InputHandle *BaseObject::getInputHandle() { return nullptr; }
router::OutputHandle *BaseObject::getOutputHandle() { return nullptr; }

void BaseObject::propSubsystemAdvanceAutomation() {}

void BaseObject::tickAutomation() {
  this->propSubsystemAdvanceAutomation();
  // Then tick events, which we want to happen before their scheduled time, enver after.
  this->scheduled_events.tick(this->context, std::static_pointer_cast<BaseObject>(this->shared_from_this()),
                              this->getAutomationTimeInSamples());
}

unsigned int BaseObject::getAutomationTimeInBlocks() {
  // Context automation has to tick before everything else, which means it's always ahead by one block.  We need to
  // recompute off the block time, which increments at the end since it tracks the current block.
  return this->getContextRaw()->getBlockTime();
}

double BaseObject::getAutomationTimeInSamples() { return this->getAutomationTimeInBlocks() * config::BLOCK_SIZE; }

void BaseObject::automationScheduleEvent(double time, unsigned long long param) {
  ScheduledEvent e{time, param};
  this->scheduled_events.addItem(e);
}

void BaseObject::automationClearScheduledEvents() { this->scheduled_events.clear(); }

void BaseObject::signalLingerStopPoint() {
  if (this->linger_reference == nullptr) {
    return;
  }

  this->context->enqueueLingerStop(std::static_pointer_cast<BaseObject>(this->linger_reference));
}

} // namespace synthizer
