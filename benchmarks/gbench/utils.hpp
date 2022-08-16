#pragma once

#include "synthizer.h"

#include <stdio.h>
#include <stdlib.h>

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

} // namespace syz_bench
