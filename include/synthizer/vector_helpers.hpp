#pragma once

#include <algorithm>
#include <memory>
#include <vector>

namespace synthizer {

/*
 * Helpers to iterate over vectors and do interesting things.
 * */

/* Operations on weak_ptr vectors. */
namespace weak_vector {

template<typename T>
bool contains(const std::vector<std::weak_ptr<T>> &v, const std::shared_ptr<T> &x) {
	for (auto &i: v) {
		if (i.lock() == x) return true;
	}

	return false;
}

/* Call the callable on all elements of the vector which are still there, and remove any weak_ptr that's not. */
template<typename T, typename CALLABLE>
void iterate_removing(std::vector<std::weak_ptr<T>> &v, CALLABLE &&c) {
	unsigned int i = 0;
	unsigned int size = v.size();

	while (i < size) {
		auto s = v[i].lock();
		if (s == nullptr) {
			std::swap(v[size-1], v[i]);
			size--;
			continue;
		}
		c(s);
		i++;
	}

	v.resize(size);
}

}

}