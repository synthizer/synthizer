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

template<typename T, typename ALLOC>
bool contains(const std::vector<std::weak_ptr<T>, ALLOC> &v, const std::shared_ptr<T> &x) {
	for (auto &i: v) {
		if (i.lock() == x) return true;
	}

	return false;
}

/* Call the callable on all elements of the vector which are still there, and remove any weak_ptr that's not. */
template<typename T, typename CALLABLE, typename ALLOC>
void iterate_removing(std::vector<std::weak_ptr<T>, ALLOC> &v, CALLABLE &&c) {
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

namespace vector_helpers {

/*
	* filter a vector. The underlying type must be at least movable. The order of the elements in the vector will change.
	*/
template<typename T, typename ALLOC, typename CALLABLE>
void filter(std::vector<T, ALLOC> &vec, CALLABLE &&callable) {
	unsigned int vsize = vec.size();
	unsigned int i = 0;
	while (i < vsize) {
		if (callable(vec[i]) == false) {
			i++;
			continue;
		}
		std::swap(vec[vsize-1], vec[i]);
		vsize--;
	}
	vec.resize(vsize);
}

/*
 * Filter a vector. The order of the elements in the vector will not change and no intermediate allocation will occur, but this is slower than regular filter
 * because removing elements requires copying everything after the removed element. Note that we have no choice but to call resize; if stdlib decides to allocate in that case,
 * we don't have much control, but it shouldn't.
 * */
template<typename T, typename ALLOC, typename CALLABLE>
void filter_stable(std::vector<T, ALLOC> &vec, CALLABLE &&callable) {
	unsigned int i = 0;
	unsigned int vsize = vec.size();
	unsigned int copyback  = 0;
	/* Advance i to the first filtered-out element. This is the fast path, for when vectors don't need filtering. */
	while (i < vsize && callable(vec[i])) {
		i++;
	}
	/* Any unfiltered element needs to be moved at least 1 back in the vector now. */
	copyback = 1;	
	/* We're on the first element to be filtered. We skip ahead to the next one. */
	i++;
	/* And subtract from vsize, outside the loop. */
	vsize--;
	while (i < vsize) {
		if (callable(vec[i])) {
			/* Copy backward by copyback. The last iteration moved previous elements out of the way. */
			vec[i-copyback] = vec[i];
		} else {
			copyback++;
			vsize--;
		}
		i++;
	}
	vec.resize(vsize);
}

}
}