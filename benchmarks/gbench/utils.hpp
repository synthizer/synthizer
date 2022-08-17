#pragma once

#include "synthizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

/**
 * Wrap a synthizer call in this macro, and it will exit the process early on error.
 * */
#define SYZ_CHECKED(x)                                                                                                 \
  do {                                                                                                                 \
    auto ret = x;                                                                                                      \
    if (ret) {                                                                                                         \
      printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());                             \
      exit(EXIT_FAILURE);                                                                                              \
    }                                                                                                                  \
  } while (0)

namespace syz_bench {

/**
 * This class will initialize synthizer and deinit synthizer at scope exit.
 *
 * used in our custom main to make sure that Synthizer is initialized for the lifetime of the process.
 * */
class SynthizerInitGuard {
public:
  SynthizerInitGuard() { SYZ_CHECKED(syz_initialize()); }

  ~SynthizerInitGuard() { syz_shutdown(); }
};

/**
 * A helper that always drops handles.
 *
 * Handles are registered with .register(), and will be destroyed in reverse order of addition when this object's
 * destructor is called.
 * */
class HandleSet {
public:
  void registerHandle(syz_Handle handle) { this->handles.push_back(handle); }

  ~HandleSet() {
    for (auto i = this->handles.rbegin(); i != this->handles.rend(); i++) {
      syz_handleDecRef(*i);
    }
  }

private:
  std::vector<syz_Handle> handles;
};
} // namespace syz_bench
