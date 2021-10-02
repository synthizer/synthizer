#pragma once

#include "synthizer/memory.hpp"

#include "pdqsort.h"

#include <algorithm>
#include <optional>

#include <stdio.h>

namespace synthizer {

/**
 * A timeline which works over anything which has a `getTime()` method on it.  The two template parameters are the type
 * of the item in the timeline and the number of items to keep which are now in the past.  The timeline will maintain a
 * pointer to the current item with the ability to access `HISTORY_LENGTH` items in the past, if possible.
 *
 * To use, call `tick` with the new time, then call `getItem()` with an offset which starts at 0 for now and proceeds
 * negative in the past (e.g. `getItem(-1)` is 1 item ago, and so on).  `getItem(0)` returns the next item which is at
 * or past the current time, e.g. it "isn't late", but it is up to the caller to determine if the item should be used.
 * Time must be monatonically increasing for now.
 *
 * One special case of `getItem()` is that it will hold at the end of the timeline if the timeline is finished, even if
 * the item at the end of the timeline is before the current time; users can and should check for this case, which
 * allows them to make sure that they set themselves to the final value of the timeline.
 *
 * The third template parameter controls how often the timeline copies items back and should probably be left at the
 * default unless `HISTORY_LENGT` should for some reason need to be longer than it.
 * */
template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD = 128> class GenericTimeline {
  static_assert(HISTORY_LENGTH != 0 && HISTORY_LENGTH < COPYBACK_THRESHOLD);

public:
  GenericTimeline() { this->items.reserve(COPYBACK_THRESHOLD * 2); }

  /**
   * Offset may be positive if you want to peak into the future,.
   * */
  std::optional<T *> getItem(int offset = 0);

  void tick(double time);
  /**
   * Overload of `tick` that allows the caller to know about late items. The callback gets passed a pointer to each item
   * in order.
   * */
  template <typename CB> void tick(double time, CB &&callback);

  void addItem(const T &item);
  template <typename IT> void addItems(IT first, IT last);

  void clear();

private:
  void sortIfNeeded();
  /* Drop items as necessary. */
  void copyBackIfNeeded();

  struct Entry {
    T item;
    unsigned int tie_breaker;
  };
  deferred_vector<Entry> items{};
  unsigned int current_item = 0;
  /* Empty timelines are sorted. */
  bool is_sorted = true;
  /* Used to make pdqsort stable. */
  unsigned int insert_counter = 0;
};

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::addItem(const T &item) {
  this->items.push_back({item, this->insert_counter++});
  if (this->items.size() > 1) {
    auto &last = this->items[this->items.size() - 1];
    auto &cur = this->items.back();
    this->is_sorted = last.item.getTime() <= cur.item.getTime();
  }
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
template <typename IT>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::addItems(IT first, IT last) {
  for (; first < last; first++) {
    this->addItem(*first);
  }
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::sortIfNeeded() {
  if (this->is_sorted) {
    return;
  }

  pdqsort_branchless(this->items.begin(), this->items.end(), [](auto &a, auto &b) {
    // This is a stable comparison that uses the insert counter to break ties. Stable comparisons give us defined
    // behavior when users specify points at the same time: the later insert wins.  it may seem like this is uncommon,
    // but user-specified writes to properties desugar to timeline oeprations and so multiuple sets in the same tick can
    // lead to this behavior.
    return std::tie(a.item.getTime(), a.tie_breaker) < std::tie(b.item.getTime(), b.tie_breaker);
  });
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::copyBackIfNeeded() {
  if (this->current_item <= COPYBACK_THRESHOLD) {
    return;
  }

  std::size_t can_lose = this->current_item - HISTORY_LENGTH;
  auto keep_start = this->items.begin() + can_lose;
  std::copy(keep_start, this->items.end(), this->items.begin());
  this->items.erase(this->items.begin() + HISTORY_LENGTH, this->items.end());
  this->current_item = HISTORY_LENGTH - 1;
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::tick(double time) {
  this->tick(time, [](auto x) { (void)x; });
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
template <typename CB>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::tick(double time, CB &&callback) {
  while (this->current_item < this->items.size() && this->items[this->current_item].item.getTime() <= time) {
    callback(&(this->items[this->current_item].item));
    this->current_item++;
  }

  this->copyBackIfNeeded();
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
std::optional<T *> GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::getItem(int offset) {
  // Bump down by 1 if we're past the end.
  unsigned int effective_item = this->current_item - (this->current_item == this->items.size());
  int actual = static_cast<int>(effective_item) + offset;
  if (actual < 0 || actual >= (int)this->items.size()) {
    return std::nullopt;
  }

  return &this->items[actual].item;
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::clear() {
  this->current_item = 0;
  this->is_sorted = true;
  this->items.clear();
}

} // namespace synthizer
