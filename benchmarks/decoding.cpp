#include <algorithm>
#include <chrono>
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
  syz_Handle context = 0, buffer = 0;
  int ecode = 0, ending = 0;
  unsigned int iterations = 10;
  std::chrono::high_resolution_clock::time_point t_start, t_end;
  std::chrono::high_resolution_clock clock;
  unsigned int total_frames = 0, frames_tmp;

  if (argc != 2) {
    printf("Specify file to decode\n");
  }

  syz_libraryConfigSetDefaults(&library_config);
  library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
  library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
  CHECKED(syz_initializeWithConfig(&library_config));

  CHECKED(syz_createContext(&context, NULL, NULL));

  t_start = clock.now();
  for (unsigned int i = 0; i < iterations; i++) {
    CHECKED(syz_createBufferFromStreamParams(&buffer, "file", argv[1], NULL, NULL, NULL));
    CHECKED(syz_bufferGetLengthInSamples(&frames_tmp, buffer));
    total_frames += frames_tmp;
    CHECKED(syz_handleDecRef(buffer));
    /* if we fail to create the new one, let's no-op the free at the bottom. */
    buffer = 0;
  }
  t_end = clock.now();

  {
    auto dur = t_end - t_start;
    auto tmp = std::chrono::duration<double>(dur);
    double secs = tmp.count();
    printf("Took %f seconds total\n", secs);
    printf("%f per decode\n", secs / iterations);
    printf("Frames per second: %f\n", total_frames / secs);
  }

end:
  ending = 1;
  CHECKED(syz_handleDecRef(buffer));
  CHECKED(syz_handleDecRef(context));
end2:
  CHECKED(syz_shutdown());
  return ecode;
}
