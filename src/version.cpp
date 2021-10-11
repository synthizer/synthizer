#include "synthizer.h"

SYZ_CAPI void syz_getVersion(unsigned int *major, unsigned int *minor, unsigned int *patch) {
  *major = SYZ_MAJOR;
  *minor = SYZ_MINOR;
  *patch = SYZ_PATCH;

  return;
}
