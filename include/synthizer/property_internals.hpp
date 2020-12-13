#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"
#include "synthizer/property_xmacros.hpp"

#include "synthizer/cells.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <variant>

namespace synthizer {

/*
 * Properties work through a DSL using X macros and a ton of magic. This file contains helpers for the magic.
 * 
 * To add a property, follow the instructions in synthizer_properties.h, then define the following macros and include property_impl.hpp (which can be done more than once per file).
 * 
 * - PROPERTY_CLASS: the class to implement property functionality for.
 * - PROPERTY_BASE: the base class one level up
 * - PROPPERTY_LIST: the macro from synthizer_properties.h that has the properties for this class.
 * 
 * For example:
 * 
 * #define PROPERTY_CLASS PannedSource
 * #define PROPERTY_BASE Source
 * #define PROPERTY_LIST PANNED_SOURCE_PROPERTIES
 * #include "synthizer/property_impl.hpp"
 * 
 * the effect is to override a number of methods from the base (ultimately BaseObject) to provide a getProperty and setProperty that use variants which back onto getXXX and setXXX functions.
 * */

class CExposable;

/* We hide this in a namespace because it's only for macro magic and shouldn't be used directly by anything else. */
namespace property_impl {

using PropertyValue = std::variant<int, double, std::shared_ptr<CExposable>, std::array<double, 3>, std::array<double, 6>>;

/* array<double, 3. doesn't work in macros because of the comma. */
using arrayd3 = std::array<double, 3>;
using arrayd6 = std::array<double, 6>;

const auto int_min = std::numeric_limits<int>::min();
const auto int_max = std::numeric_limits<int>::max();
const auto double_min = std::numeric_limits<double>::lowest();
const auto double_max = std::numeric_limits<double>::max();
}

/*
 * these are property containers which orchestrate the storage/retrieval of properties in a threadsafe manner, assuming that
 * write is only ever called from one thread at a time.
 * */
template<typename T>
class AtomicProperty {
	public:
	AtomicProperty(): AtomicProperty(T()) {}
	AtomicProperty(const T&& value): field(value) {}

	T read() { return this->field.load(std::memory_order_acquire); }
	void write(T value) { this->field.store(value, std::memory_order_release); }
	private:
	std::atomic<T> field{};
};

using IntProperty = AtomicProperty<int>;
using DoubleProperty = AtomicProperty<double>;

template<typename T>
class LatchProperty {
	LatchProperty(): LatchProperty(T()) {}
	LatchProperty(const T&& value): field(value) {}

	T read() { return this->field.read(); }
	void write(const T&& value) { this->field.write(value); }

	private:
	LatchCell<T> field;
};

using Double3Property = LatchProperty<std::array<double, 3>>;
using Double6Property = LatchProperty<std::array<double, 6>>;

template<typename T>
class ObjectProperty {
	public:
	std::weak_ptr<T> read() {
		std::weak_ptr<T> out;
		this->lock();
		out = this->field;
		this->unlock();
		return out;
	}

	void write(const std::weak_ptr<T> &value) {
		this->lock();
		this->field = value;
		this->unlock();
	}

	void lock() {

		int old = 0;
		while (this->spinlock.compare_exchange_strong(old, 1, std::memory_order_acquire, std::memory_order_relaxed) != true) {
			old = 0;
			/* Don't yield because this may be the audio thread. */
		}
	}

	void unlock() {
		this->spinlock.store(0, std::memory_order_release);
	}

	private:
	std::atomic<int> spinlock = 0;
	std::weak_ptr<T> field{};
};

/* Used when expanding the X-lists. */
#define P_INT_MIN property_impl::int_min
#define P_INT_MAX property_impl::int_max
#define P_DOUBLE_MIN property_impl::double_min
#define P_DOUBLE_MAX property_impl::double_max

}
