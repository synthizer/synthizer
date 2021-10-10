/**
 * Helpers for all examples.
 * */
#include "synthizer.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECKED(x)                                                                                                     \
  do {                                                                                                                 \
    int ret = x;                                                                                                       \
    if (ret) {                                                                                                         \
      printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());                             \
      syz_shutdown();                                                                                                  \
      exit(EXIT_FAILURE);                                                                                              \
    }                                                                                                                  \
  } while (0)
