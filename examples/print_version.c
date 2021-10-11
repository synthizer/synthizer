#include "synthizer.h"

#include <stdio.h>

int main() {
  unsigned int major, minor, patch;

  syz_getVersion(&major, &minor, &patch);
  printf("%u.%u.%u\n", major, minor, patch);

  return 0;
}
