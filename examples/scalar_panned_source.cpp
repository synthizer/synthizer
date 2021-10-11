/**
 * Demonstrates basic use of a ScalarPannedSource by moving a sound from left to right and back.  The sourec is
 * configured to use HRTF>
 *
 * Uses C++ because MSVC doesn't support threads.h and we need to sleep.
 * */
#include "example_common.h"

#include "synthizer.h"
#include "synthizer_constants.h"

#include <chrono>
#include <thread>

#include <stdio.h>

const unsigned int ITERATIONS_PER_CYCLE = 200;
const unsigned int HALF_CYCLE = ITERATIONS_PER_CYCLE / 2;
const unsigned int SLEEP_MS = 20;

int main(int argc, char *argv[]) {
  struct syz_LibraryConfig library_config;
  syz_Handle context = 0, generator = 0, source = 0, buffer = 0;
  struct syz_Event event;
  int object_type;

  if (argc != 2) {
    printf("Usage: %s <path>\n", argv[0]);
    return 1;
  }

  syz_libraryConfigSetDefaults(&library_config);
  library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
  library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
  CHECKED(syz_initializeWithConfig(&library_config));

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL, NULL));
  CHECKED(syz_createScalarPannedSource(&source, context, SYZ_PANNER_STRATEGY_HRTF, 0.0, NULL, NULL, NULL));
  CHECKED(syz_sourceAddGenerator(source, generator));

  CHECKED(syz_createBufferFromFile(&buffer, argv[1], NULL, NULL));
  CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));
  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));

  for (unsigned int i = 0; i < ITERATIONS_PER_CYCLE * 10; i++) {
    unsigned int cycle_offset = i % ITERATIONS_PER_CYCLE;
    unsigned int half_cycle_offset = i % HALF_CYCLE;
    double offset = 2.0 * (double)half_cycle_offset / HALF_CYCLE;
    double val = -1.0 + offset;

    if (i % ITERATIONS_PER_CYCLE >= HALF_CYCLE) {
      val *= -1.0;
    }
    CHECKED(syz_setD(source, SYZ_P_PANNING_SCALAR, val));
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
  }

  syz_handleDecRef(context);
  syz_handleDecRef(generator);
  syz_handleDecRef(buffer);
  syz_handleDecRef(source);
  syz_shutdown();

  return 0;
}
