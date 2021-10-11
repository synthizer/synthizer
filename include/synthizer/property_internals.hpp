#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/property_automation_timeline.hpp"
#include "synthizer/property_xmacros.hpp"

#include "synthizer/cells.hpp"
#include "synthizer/property_automation_timeline.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <variant>

namespace synthizer {

/*
 * Properties work through a DSL using X macros and a ton of magic. This file contains helpers for the magic.
 *
 * To add a property, follow the instructions in synthizer_properties.h, then define the following macros and include
 * property_impl.hpp (which can be done more than once per file).
 *
 * - PROPERTY_CLASS: the class to implement property functionality for.
 * - PROPERTY_BASE: the base class one level up
 * - PROPPERTY_LIST: the macro from synthizer_properties.h that has the properties for this class.
 *
 * For example:
 *
 * #define PROPERTY_CLASS PannedSource #define PROPERTY_BASE Source #define PROPERTY_LIST PANNED_SOURCE_PROPERTIES
 * #include "synthizer/property_impl.hpp"
 *
 * the effect is to override a number of methods from the base (ultimately BaseObject) to provide a getProperty and
 * setProperty that use variants which back onto getXXX and setXXX functions.
 * */

class CExposable;
class BaseObject;

/* We hide this in a namespace because it's only for macro magic and shouldn't be used directly by anything else. */
namespace property_impl {

using PropertyValue = std::variant<int, double, std::shared_ptr<CExposable>, std::array<double, 3>,
                                   std::array<double, 6>, struct syz_BiquadConfig>;

/* array<double, 3. doesn't work in macros because of the comma. */
using arrayd3 = std::array<double, 3>;
using arrayd6 = std::array<double, 6>;

const auto int_min = std::numeric_limits<int>::min();
const auto int_max = std::numeric_limits<int>::max();
const auto double_min = std::numeric_limits<double>::lowest();
const auto double_max = std::numeric_limits<double>::max();
} // namespace property_impl

/* By default C++17 doesn't give us comparison for structs. */
inline bool operator==(const struct syz_BiquadConfig &l, const struct syz_BiquadConfig &r) {
  return std::tie(l._b0, l._b1, l._b2, l._a1, l._a2, l._gain, l._is_wire) ==
         std::tie(r._b0, r._b1, r._b2, r._a1, r._a2, r._gain, r._is_wire);
}

inline bool operator!=(const struct syz_BiquadConfig &l, const struct syz_BiquadConfig &r) { return !(l == r); }

/*
 * these are property containers which orchestrate the storage/retrieval of properties in a threadsafe manner, assuming
 * that write is only ever called from one thread at a time.
 * */
template <typename T> class AtomicProperty {
public:
  AtomicProperty() : AtomicProperty(T()) {}
  explicit AtomicProperty(const T &value) : field(value) {}

  T read() const { return this->field.load(std::memory_order_acquire); }
  void write(T value, bool track_change = true) {
    auto old = this->field.load(std::memory_order_relaxed);
    this->field.store(value, std::memory_order_release);
    if (track_change && old != value) {
      this->changed = true;
    }
  }

  bool acquire(T *out) {
    *out = this->read();
    bool _changed = this->changed;
    this->changed = false;
    return _changed;
  }
  void markUnchanged() { this->changed = false; }

private:
  std::atomic<T> field{};
  bool changed = true;
};

using IntProperty = AtomicProperty<int>;

class DoubleProperty : AtomicProperty<double> {
public:
  DoubleProperty(double dv) : AtomicProperty<double>(dv) {}

  double read() const { return AtomicProperty::read(); }
  void write(double val, bool track_changes = true) { AtomicProperty::write(val, track_changes); }
  void writeAutomated(double automation_time, double val) {
    PropertyAutomationPoint<1> point{automation_time, SYZ_INTERPOLATION_TYPE_NONE, std::array<double, 1>{{val}}};
    this->timeline.addPoint(point);
  }
  bool acquire(double *val) { return AtomicProperty::acquire(val); }
  void markUnchanged() { AtomicProperty::markUnchanged(); }

  PropertyAutomationTimeline<1> *getTimeline() { return &this->timeline; }

  // Relies on the time which is passed to be one block, and to always advance by one block (e.g. time must be an
  // integer number of samples and a multiple of config::BLOCK_SIZE).
  //
  // This isn't ideal, but automation is complicated and we need to be able to give callers the value of the property at
  // the next block, so they can fade properly.
  void tickAutomation(double time) {
    this->timeline.tick(time);
    auto val = this->timeline.getValue();
    if (val) {
      this->write((*val)[0]);
    }

    this->timeline.tick(time + config::BLOCK_SIZE);
    this->next_block_value = std::nullopt;
    auto maybe_v = this->timeline.getValue();
    if (maybe_v) {
      this->next_block_value = (*maybe_v)[0];
    }
  }

  // nullopt if the next block doesn't have a value yet because automation is finished.
  std::optional<double> getValueForNextBlock() { return this->next_block_value; }

private:
  PropertyAutomationTimeline<1> timeline;
  std::optional<double> next_block_value;
};

/**
 * A property backed by LatchCell. Everything but read must be called from the audio thread.
 * */
template <typename T> class LatchProperty {
public:
  LatchProperty() : LatchProperty(T()) {}
  explicit LatchProperty(const T &value) : field(value) {}

  T read() const { return this->field.read(); }
  void write(const T &value, bool track_change = true) {
    auto old = this->read();
    this->field.write(value);
    if (track_change && old != value) {
      this->changed = true;
    }
  }
  bool acquire(T *out) {
    *out = this->read();
    bool _changed = this->changed;
    this->changed = false;
    return _changed;
  }
  void markUnchanged() { this->changed = false; }

private:
  mutable LatchCell<T> field;
  bool changed = true;
};

class Double3Property : LatchProperty<std::array<double, 3>> {
public:
  Double3Property(const std::array<double, 3> &dv) : LatchProperty<std::array<double, 3>>(dv) {}

  std::array<double, 3> read() const { return LatchProperty::read(); }
  void write(const std::array<double, 3> &val, bool track_changes = true) { LatchProperty::write(val, track_changes); }
  void writeAutomated(double automation_time, const std::array<double, 3> &val) {
    PropertyAutomationPoint<3> point{automation_time, SYZ_INTERPOLATION_TYPE_NONE, val};
    this->timeline.addPoint(point);
  }
  bool acquire(std::array<double, 3> *val) { return LatchProperty::acquire(val); }
  void markUnchanged() { LatchProperty::markUnchanged(); }

  PropertyAutomationTimeline<3> *getTimeline() { return &this->timeline; }

  void tickAutomation(double time) {
    this->timeline.tick(time);
    auto val = this->timeline.getValue();
    if (val) {
      auto [a, b, c] = *val;
      this->write({a, b, c});
    }
  }

private:
  PropertyAutomationTimeline<3> timeline{};
};

class Double6Property : LatchProperty<std::array<double, 6>> {
public:
  Double6Property(const std::array<double, 6> &dv) : LatchProperty<std::array<double, 6>>(dv) {}

  std::array<double, 6> read() const { return LatchProperty::read(); }
  void write(const std::array<double, 6> &val, bool track_changes = true) { LatchProperty::write(val, track_changes); }
  void writeAutomated(double automation_time, const std::array<double, 6> &val) {
    PropertyAutomationPoint<6> point{automation_time, SYZ_INTERPOLATION_TYPE_NONE, val};
    this->timeline.addPoint(point);
  }
  bool acquire(std::array<double, 6> *val) { return LatchProperty::acquire(val); }
  void markUnchanged() { LatchProperty::markUnchanged(); }

  PropertyAutomationTimeline<6> *getTimeline() { return &this->timeline; }

  void tickAutomation(double time) {
    this->timeline.tick(time);
    auto val = this->timeline.getValue();
    if (val) {
      auto [a, b, c, d, e, f] = *val;
      this->write({a, b, c, d, e, f});
    }
  }

private:
  PropertyAutomationTimeline<6> timeline{};
};

/*
 * This is threadsafe, and we have kept that functionality even though we don't expose it publicly.
 * If uncontended, this is about 3 atomic operations for a read, and it's useful to be threadsafe because all the other
 * properties are, but using it from multiple threads can trivially cause priority inversion issues.
 * */
template <typename T> class ObjectProperty {
public:
  std::weak_ptr<T> read() const {
    std::weak_ptr<T> out;
    this->lock();
    out = this->field;
    this->unlock();
    return out;
  }

  void write(const std::weak_ptr<T> &value, bool track_change = true) {
    this->lock();
    this->field = value;
    this->unlock();
    if (track_change) {
      this->changed = true;
    }
  }

  bool acquire(std::weak_ptr<T> *out) {
    *out = this->read();
    bool _changed = this->changed;
    this->changed = false;
    return _changed;
  }

  void markUnchanged() { this->changed = false; }

private:
  void lock() const {
    int old = 0;
    while (this->spinlock.compare_exchange_strong(old, 1, std::memory_order_acquire, std::memory_order_relaxed) !=
           true) {
      old = 0;
      /* Don't yield because this may be the audio thread. */
    }
  }

  void unlock() const { this->spinlock.store(0, std::memory_order_release); }

  mutable std::atomic<int> spinlock = 0;
  std::weak_ptr<T> field{};
  bool changed = true;
};

class BiquadProperty : public LatchProperty<syz_BiquadConfig> {
public:
  BiquadProperty() : LatchProperty() {
    syz_BiquadConfig default_value{};
    /* Internal detail: wire never fails. */
    syz_biquadDesignIdentity(&default_value);
    this->write(default_value);
  }
};

/* Used when expanding the X-lists. */
#define P_INT_MIN property_impl::int_min
#define P_INT_MAX property_impl::int_max
#define P_DOUBLE_MIN property_impl::double_min
#define P_DOUBLE_MAX property_impl::double_max

/**
 * Callable to be wrapped in a command which sets properties.
 * */
void setPropertyCmd(int property, std::weak_ptr<BaseObject> target_weak, property_impl::PropertyValue value);

} // namespace synthizer
