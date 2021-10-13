/* Test that repeatedly seeks to the beginning. Not automated, needs to be run manually and listened to. */
#include <algorithm>
#include <array>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include <cmath>
#include <cstddef>

#include "synthizer.h"
#include "synthizer_constants.h"

#define PI 3.1415926535
#define CHECKED(x)                                                                                                     \
  do {                                                                                                                 \
    auto ret = x;                                                                                                      \
    if (ret) {                                                                                                         \
      printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());                             \
      ecode = 1;                                                                                                       \
      if (ending == 0)                                                                                                 \
        goto end;                                                                                                      \
      else                                                                                                             \
        goto end2;                                                                                                     \
    }                                                                                                                  \
  } while (0)

int main(int argc, char *argv[]) {
  struct syz_LibraryConfig library_config;
  syz_Handle context = 0, generator = 0, source = 0, buffer = 0;
  int ecode = 0, ending = 0;
  double angle = 0.0, angle_per_second = 45.0;

  if (argc != 2) {
    printf("Specify file\n");
    return 1;
  }

  syz_libraryConfigSetDefaults(&library_config);
  library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
  library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
  CHECKED(syz_initializeWithConfig(&library_config));

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_createDirectSource(&source, context, NULL, NULL, NULL));
  CHECKED(syz_createBufferFromStreamParams(&buffer, "file", argv[1], NULL, NULL, NULL));
  CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL, NULL));
  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
  CHECKED(syz_sourceAddGenerator(source, generator));

  while (true) {
    CHECKED(syz_setD(generator, SYZ_P_PLAYBACK_POSITION, 0.0));
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }

end:
  ending = 1;
  CHECKED(syz_handleDecRef(source));
  CHECKED(syz_handleDecRef(generator));
  CHECKED(syz_handleDecRef(context));
end2:
  CHECKED(syz_shutdown());
  return ecode;
}
