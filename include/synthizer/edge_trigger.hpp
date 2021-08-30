#pragma once

#include <cassert>
#include <type_traits>
#include <utility>

namespace synthizer {

/**
 * An EdgeTrigger provides trigger notifications when a boolean value
 * switches from false to true, true to false, or both.  This is named for and functions like
 * the electronics equivalent.
 *
 * To use, create an instance of EdgeTrigger, then call evaluate once a block (or on whatever interval
 * is appropriate).
 *
 * This is used as part of the event system, which generally needs to fire events when a condition changes.
 *
 * It is permissible for the callbacks here to have side effects.  This interface isn't intended to be pure, only to get
 * a particular coding idiom out of the way of more important logic.  In particular the condition is called on every
 * call to evaluate.
 * */

enum class EdgeTriggerType {
  /* Trigger when going from false to true. */
  Up,
  /* Trigger when going from true to false. */
  Down,
  /* Trigger whenever the condition changes. */
  Both,
};

/**
 * The type-erased interface and functions.  Though synthizer tries to allocate inline
 * this is one place where a convenient interface requires allocating on the heap.
 *
 * This probably isn't the class you want to use; see EdgeTrigger, below, which provides an abstraction that hides the
 * heap allocations,.
 * */
class EdgeTriggerBase {
public:
  EdgeTriggerBase(EdgeTriggerType type, bool initial) : type(type), last_value(initial) {}

  virtual ~EdgeTriggerBase() {}

  /* Methods overridden in the concrete implementation. */
  virtual bool evaluateCondition() = 0;
  virtual void trigger() = 0;

  bool shouldTriggerUp() { return this->type != EdgeTriggerType::Down; }

  bool shouldTriggerDown() { return this->type != EdgeTriggerType::Up; }

  void evaluate() {
    bool new_value = this->evaluateCondition();

    if ((this->last_value == false && new_value == true && this->shouldTriggerUp()) ||
        (this->last_value == true && new_value == false && this->shouldTriggerDown())) {
      this->trigger();
    }

    this->last_value = new_value;
  }

private:
  EdgeTriggerType type;
  bool last_value;
};

template <typename CONDITION_CB, typename TRIGGER_CB> class ConcreteEdgeTrigger : public EdgeTriggerBase {
public:
  template <typename CB1, typename CB2>
  ConcreteEdgeTrigger(EdgeTriggerType type, const CB1 &&condition, const CB2 &&trigger)
      : EdgeTriggerBase(type, condition()), condition_callback(std::forward<const CB1>(condition)),
        trigger_callback(std::forward<const CB2>(trigger)) {}

  bool evaluateCondition() override { return this->condition_callback(); }

  void trigger() override { this->trigger_callback(); }

private:
  CONDITION_CB condition_callback;
  TRIGGER_CB trigger_callback;
};

/**
 * An edge trigger. Pass two callbacks to the constructor, one to evaluate the condition and one
 * to call when the trigger fires, then call evaluate periodically.
 * */
class EdgeTrigger {
public:
  EdgeTrigger() : trigger(nullptr) {}

  template <typename CONDITION_CB, typename TRIGGER_CB>
  EdgeTrigger(EdgeTriggerType type, const CONDITION_CB &&condition_callback, const TRIGGER_CB &&trigger_callback) {
    this->configureEdgeTrigger(type, std::forward(condition_callback), std::forward(trigger_callback));
  }

  template <typename CONDITION_CB, typename TRIGGER_CB>
  void configureEdgeTrigger(EdgeTriggerType type, const CONDITION_CB &&condition_callback,
                            const TRIGGER_CB &&trigger_callback) {
    this->trigger = new ConcreteEdgeTrigger<std::remove_reference_t<CONDITION_CB>, std::remove_reference_t<TRIGGER_CB>>(
        type, std::forward<const CONDITION_CB>(condition_callback), std::forward<const TRIGGER_CB>(trigger_callback));
  }

  ~EdgeTrigger() { delete this->trigger; }

  void evaluate() {
    assert(this->trigger != nullptr && "EdgeTrigger not properly initialized");
    this->trigger->evaluate();
  }

private:
  EdgeTriggerBase *trigger;
};

} // namespace synthizer
