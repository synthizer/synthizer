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
  syz_Handle context = 0, generator = 0, source = 0, buffer = 0, effect = 0;
  int ecode = 0, ending = 0;
  double angle = 0.0, angle_per_second = 45.0;
  struct syz_RouteConfig route_config;
  struct syz_BiquadConfig filter;

  if (argc != 2) {
    printf("Usage: wav_test <path>\n");
    return 1;
  }

  syz_libraryConfigSetDefaults(&library_config);
  library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
  library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
  CHECKED(syz_initializeWithConfig(&library_config));

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_setI(context, SYZ_P_DEFAULT_PANNER_STRATEGY, SYZ_PANNER_STRATEGY_HRTF));

  CHECKED(syz_contextEnableEvents(context));
  CHECKED(syz_createSource3D(&source, context, SYZ_PANNER_STRATEGY_DELEGATE, 0.0, 0.0, 0.0, NULL, NULL, NULL));
  CHECKED(syz_createBufferFromStreamParams(&buffer, "file", argv[1], NULL, NULL, NULL));
  CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL, NULL));
  CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));
  // CHECKED(syz_setD(generator, SYZ_P_PITCH_BEND, 2.0));
  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
  CHECKED(syz_sourceAddGenerator(source, generator));

  CHECKED(syz_initRouteConfig(&route_config));
  CHECKED(syz_createGlobalFdnReverb(&effect, context, NULL, NULL, NULL));
  CHECKED(syz_routingConfigRoute(context, source, effect, &route_config));

  // std::this_thread::sleep_for(std::chrono::seconds(2));
  // CHECKED(syz_pause(source));
  // std::this_thread::sleep_for(std::chrono::seconds(5));
  CHECKED(syz_play(source));

  while (true) {
    syz_Event event;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECKED(syz_setD(effect, SYZ_P_LATE_REFLECTIONS_MODULATION_DEPTH, 0.0));
    angle += 0.01 * angle_per_second;
    angle = fmod(angle, 360.0);
    double rad = angle * PI / 180.0;
    double y = cos(rad);
    double x = sin(rad);
    CHECKED(syz_setD3(source, SYZ_P_POSITION, x, y, 0.0));

    CHECKED(syz_contextGetNextEvent(&event, context, 0));
    syz_eventDeinit(&event);
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
