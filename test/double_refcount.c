/**
 * Test that two calls to syz_incRef followed by 2 calls to syz_decRef don't
 * crash etc.
 * */
#include <stdio.h>

#include "synthizer.h"

#define CHECK(X)                                                                                                       \
  if ((X) != 0) {                                                                                                      \
    printf("Failed: " #X);                                                                                             \
    return 1;                                                                                                          \
  }

int main() {
  syz_Handle h;

  CHECK(syz_initialize());
  CHECK(syz_createContext(&h, NULL, NULL));
  CHECK(syz_handleIncRef(h));
  CHECK(syz_handleDecRef(h));
  CHECK(syz_handleDecRef(h));
  CHECK(syz_shutdown());
  return 0;
}
