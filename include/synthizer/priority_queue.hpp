#pragma once

#include "synthizer/memory.hpp"
#include "synthizer/vector_helpers.hpp"

#include <algorithm>
#include <tuple>

namespace synthizer {

namespace {
template <typename PRIO, typename ELEM>
bool heap_cmp(const std::tuple<PRIO, ELEM> &a, const std::tuple<PRIO, ELEM> &b) {
  return std::get<0>(b) < std::get<0>(a);
}

} // namespace

/**
 * A priority queue sorting items such that lower numbers are lower priority. Used primarily
 * to hold scheduling information, e.g. linger timeouts.
 * */
template <typename PRIO, typename ELEM> class PriorityQueue {
public:
  PriorityQueue() {
    /**
     * This is used in the audio thread. Let's avoid allocation by reserving. We can grow if we go
     * over, but in practice this should be large enough for most uses.
     * */
    this->elements.reserve(1024);
  }

  void push(PRIO priority, const ELEM &element) {
    this->elements.emplace_back(priority, element);
    std::push_heap(this->elements.begin(), this->elements.end(), heap_cmp<PRIO, ELEM>);
  }

  /**
   * Pop an element from the heap, returning whether an element was popped and calling the provided closure with the
   * element.
   * */
  template <typename CL> bool pop(CL &&closure) {
    if (this->elements.empty()) {
      return false;
    }
    std::pop_heap(this->elements.begin(), this->elements.end(), heap_cmp<PRIO, ELEM>);
    closure(this->elements.pop_back());
    return true;
  }

  /**
   * Pop elements until the provided closure returns false, or until no elements remain.
   * */
  template <typename CL> void popWhile(CL &&closure) {
    while (this->pop(closure))
      ;
  }

  /**
   * Pop until reaching an element with priority greater than that specified.
   * */
  template <typename CL> void popUntilPriority(PRIO priority, CL &&closure) {
    while (true) {
      if (this->elements.empty()) {
        return;
      }
      std::pop_heap(this->elements.begin(), this->elements.end(), heap_cmp<PRIO, ELEM>);
      if (std::get<0>(this->elements.back()) > priority) {
        std::push_heap(this->elements.begin(), this->elements.end(), heap_cmp<PRIO, ELEM>);
        return;
      }
      auto [prio, elem] = this->elements.back();
      elements.pop_back();
      closure(prio, elem);
    }
  }

  /**
   * Filter items in the queue using the specified closure, as with vector_helpers.hpp.
   * Then reheapify the queue.
   *
   * The closure gets (priority, element) pairs.
   * */
  template <typename CL> void filterAllItems(CL &&closure) {
    bool did_filter = false;
    vector_helpers::filter(this->elements, [&](auto &element) {
      bool ret = closure(std::get<0>(element), std::get<1>(element));
      did_filter |= ret;
      return ret;
    });
    /**
     * Skip making the heap if the vector is empty or if no filtering
     * took place. The latter is an optimization to prevent work on the audio thread when
     * nothing is dropped from the queue, which we expect to be the common
     * case.
     * */
    if (this->elements.empty() || did_filter == false) {
      return;
    }
    std::make_heap(this->elements.begin(), this->elements.end(), heap_cmp<PRIO, ELEM>);
  }

private:
  deferred_vector<std::tuple<PRIO, ELEM>> elements;
};

} // namespace synthizer
