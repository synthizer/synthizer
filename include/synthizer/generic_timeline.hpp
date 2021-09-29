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

  /**
   * Returns true if this timeline will no longer yield new items.
   * */
  bool isFinished();

  void clear();

private:
  void sortIfNeeded();
  /* Drop items as necessary. */
  void copyBackIfNeeded();

  deferred_vector<T> items{};
  unsigned int current_item = 0;
  /* Empty timelines are sorted. */
  bool is_sorted = true;
  bool finished = true;
};

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::addItem(const T &item) {
  this->items.push_back(item);
  if (this->items.size() > 1) {
    auto &last = this->items[this->items.size() - 1];
    auto &cur = this->items.back();
    this->is_sorted = last.getTime() <= cur.getTime();
  }
  this->finished = false;
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

  pdqsort_branchless(this->items.begin(), this->items.end(),
                     [](auto &a, auto &b) { return a.getTime() < b.getTime(); });
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::copyBackIfNeeded() {
  if (this->current_item <= COPYBACK_THRESHOLD) {
    return;
  }

  auto start = this->current_item - HISTORY_LENGTH;
  std::copy(this->items.begin() + start, this->items.end(), this->items.begin());
  // We got rid of everythin before start and have an extra start - 1 at the end.
  this->items.erase(this->items.end() - start + 1, this->items.end());
  // Now the beginning of the vector is the history, then the current item.
  this->current_item = HISTORY_LENGTH;
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::tick(double time) {
  this->tick(time, [](auto x) { (void)x; });
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
template <typename CB>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::tick(double time, CB &&callback) {
  while (this->current_item < this->items.size() && this->items[this->current_item].getTime() <= time) {
    callback(&this->items[this->current_item]);
    this->current_item++;
  }

  this->finished = (this->current_item >= this->items.size());
  if (this->finished) {
    return;
  }

  this->copyBackIfNeeded();
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
bool GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::isFinished() {
  return this->finished;
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
std::optional<T *> GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::getItem(int offset) {
  int actual = static_cast<int>(this->current_item) + offset;
  if (actual < 0 || actual > (int)this->items.size()) {
    return std::nullopt;
  }

  return &this->items[actual];
}

template <typename T, unsigned int HISTORY_LENGTH, unsigned int COPYBACK_THRESHOLD>
void GenericTimeline<T, HISTORY_LENGTH, COPYBACK_THRESHOLD>::clear() {
  this->current_item = 0;
  this->finished = true;
  this->is_sorted = true;
  this->items.clear();
}

} // namespace synthizer
