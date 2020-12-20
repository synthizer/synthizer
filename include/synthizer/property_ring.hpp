#pragma once

#include "synthizer/at_scope_exit.hpp"
#include "synthizer/base_object.hpp"
#include "synthizer/mpsc_ring.hpp"
#include "synthizer/property_internals.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

namespace synthizer {


struct PropertyRingEntry {
	int property;
	std::weak_ptr<BaseObject> target;
	property_impl::PropertyValue value;
};

template<std::size_t capacity>
class PropertyRing {
	public:
	/* Returns false if the ring is full. */
	template<typename T>
	bool enqueue(const std::shared_ptr<BaseObject> &obj, int property, T&& value);

	void applyAll();

	private:
	MpscRing<PropertyRingEntry, capacity> ring{};
};

template<std::size_t capacity>
template<typename T>
bool PropertyRing<capacity>::enqueue(const std::shared_ptr<BaseObject> &obj, int property, T&& value) {
	return this->ring.write([&](auto &entry) {
		entry.property = property;
		entry.target = obj;
		entry.value = value;
	});
}

template<std::size_t capacity>
void PropertyRing<capacity>::applyAll() {
	this->ring.processAll([&](auto &entry) {
		auto target = entry.target.lock();
		if (target == nullptr) {
			return;
		}

		target->setProperty(entry.property, entry.value);
	});
}

}
