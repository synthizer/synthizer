/**
 * Helpers for all examples.
 * */

#include <stdio.h>

#define CHECKED(x)                                                                                                     \
  do {                                                                                                                 \
    int ret = x;                                                                                                       \
    if (ret) {                                                                                                         \
      printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());                             \
      ecode = 1;                                                                                                       \
      goto end;                                                                                                        \
    }                                                                                                                  \
  } while (0)
