#pragma once

#include <algorithm>
#include <memory>
#include <vector>

namespace synthizer {

/*
 * Helpers to iterate over vectors and do interesting things.
 * */

/**
 * Operations on weak_ptr vectors, and anything else
 * implementing subsets of the weak_ptr interface (e.g. `GeneratorRef`).
 *
 * Functions below indicate what functionality they need.
 * */
namespace weak_vector {

/**
 * Find out if an element is contained in the vector and is still alive.
 *
 * Requires `T` to implement lock and `expired`.
 * */
template <typename T, typename C, typename ALLOC>
bool contains(const std::vector<T, ALLOC> &v, const std::shared_ptr<C> &x) {
  for (auto &i : v) {
    if (i.expired()) {
      continue;
    }
    if (i.lock() == x) {
      return true;
    }
  }

  return false;
}

/**
 * Call the callable on all elements of the vector which are still there, and remove any which are expired.
 *
 * Requires `T` to implement `expired` and `lock`.
 * */
template <typename T, typename CALLABLE, typename ALLOC> void iterate_removing(std::vector<T, ALLOC> &v, CALLABLE &&c) {
  unsigned int i = 0;
  unsigned int size = v.size();

  while (i < size) {
    decltype(v[i].lock()) s;
    if (v[i].expired() || (s = v[i].lock()) == nullptr) {
      std::swap(v[size - 1], v[i]);
      size--;
      continue;
    }
    c(s);
    i++;
  }

  v.resize(size);
}

} // namespace weak_vector

namespace vector_helpers {

/*
 * filter a vector. The underlying type must be at least movable. The order of the elements in the vector will change.
 */
template <typename T, typename ALLOC, typename CALLABLE> void filter(std::vector<T, ALLOC> &vec, CALLABLE &&callable) {
  unsigned int vsize = vec.size();
  unsigned int i = 0;
  while (i < vsize) {
    if (callable(vec[i]) == false) {
      i++;
      continue;
    }
    std::swap(vec[vsize - 1], vec[i]);
    vsize--;
  }
  vec.resize(vsize);
}

/*
 * Filter a vector. The order of the elements in the vector will not change and no intermediate allocation will occur,
 * but this is slower than regular filter because removing elements requires copying everything after the removed
 * element. Note that we have no choice but to call resize; if stdlib decides to allocate in that case, we don't have
 * much control, but it shouldn't.
 * */
template <typename T, typename ALLOC, typename CALLABLE>
void filter_stable(std::vector<T, ALLOC> &vec, CALLABLE &&callable) {
  unsigned int i = 0;
  unsigned int vsize = vec.size();
  unsigned int copyback = 0;
  while (i < vsize) {
    if (callable(vec[i])) {
      /* Copy backward by copyback. The last iteration moved previous elements out of the way. */
      vec[i - copyback] = vec[i];
    } else {
      copyback++;
    }
    i++;
  }
  vec.resize(vsize - copyback);
}

} // namespace vector_helpers
} // namespace synthizer