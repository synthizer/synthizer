/**
 * Demonstrates basic events.
 *
 * Uses C++ because MSVC doesn't support threads.h and we need to sleep.
 * */
#include "example_common.h"

#include "synthizer.h"
#include "synthizer_constants.h"

#include <chrono>
#include <thread>

#include <stdio.h>

int main(int argc, char *argv[]) {
  struct syz_LibraryConfig library_config;
  syz_Handle context = 0, generator = 0, source = 0, buffer = 0, stream = 0;
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
  CHECKED(syz_createDirectSource(&source, context, NULL, NULL, NULL));
  CHECKED(syz_sourceAddGenerator(source, generator));

  CHECKED(syz_createBufferFromFile(&buffer, argv[1], NULL, NULL));
  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));

  CHECKED(syz_contextEnableEvents(context));

  while (true) {
    CHECKED(syz_contextGetNextEvent(&event, context, 0));
    if (event.type == SYZ_EVENT_TYPE_INVALID) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    /*
     * You can deinit anywhere as long as something somewhere keeps the handles alive, but #75 had a bug that this
     * example was used to uncover.
     */
    syz_eventDeinit(&event);
    printf("Got event type %i\n", event.type);
    CHECKED(syz_handleGetObjectType(&object_type, event.source));
    printf("Handle %llu is of type %i\n", event.source, object_type);

    if (event.type == SYZ_EVENT_TYPE_FINISHED) {
      printf("Finished playing\n");
      break;
    }
  }

  syz_handleDecRef(context);
  syz_handleDecRef(generator);
  syz_handleDecRef(buffer);
  syz_handleDecRef(source);
  syz_handleDecRef(stream);
  syz_shutdown();

  return 0;
}
