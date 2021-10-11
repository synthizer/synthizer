/**
 * Demonstrates using automation to move a source in a circle forever, by refreshing the automation commands using
 * events.
 *
 * Uses C++ for sleeping.
 *
 * This works by enqueueing up some number of movement steps to move a sourcec around the listener's head using
 * trigonometry then by enqueueing an event that contains the number of iterations we've done so far.  This isn't a
 * particularly practical example, but demonstrates most of the automation API's surface area.
 * */
#include "example_common.h"

#include "synthizer.h"
#include "synthizer_constants.h"

#include <chrono>
#include <thread>

#include <math.h>
#include <stdio.h>
#include <string.h>

static const unsigned int STEPS_BEFORE_WAIT = 10;
static const double DUR_PER_STEP = 0.1;
static const double DEGS_PER_STEP = 5.0;
static const double PI = 3.141592653589793;

/**
 * Enqueue some automation including the event to drive things forward.  Uses iters_so_far to maintain state, which we
 * read back from the event this schedules.
 * */
int enqueueAutomation(syz_Handle context, syz_Handle source, double timebase, unsigned int iters_so_far) {
  syz_Handle batch = 0;
  syz_AutomationCommand cmd;
  syz_AutomationPoint point;

  CHECKED(syz_createAutomationBatch(&batch, context, NULL, NULL));

  for (unsigned int i = 0; i < STEPS_BEFORE_WAIT; i++) {
    memset(&cmd, 0, sizeof(cmd));
    memset(&point, 0, sizeof(point));

    unsigned int iter = iters_so_far + i;

    point.values[0] = 10.0 * sin(iter * DEGS_PER_STEP * PI / 180.0);
    point.values[1] = 10.0 * cos(iter * DEGS_PER_STEP * PI / 180.0);

    cmd.type = SYZ_AUTOMATION_COMMAND_APPEND_PROPERTY;
    cmd.time = timebase + iter * DUR_PER_STEP;
    cmd.params.append_to_property.property = SYZ_P_POSITION;
    cmd.params.append_to_property.point = point;
    cmd.target = source;

    CHECKED(syz_automationBatchAddCommands(batch, 1, &cmd));
  }

  memset(&cmd, 0, sizeof(cmd));
  cmd.type = SYZ_AUTOMATION_COMMAND_SEND_USER_EVENT;
  cmd.params.send_user_event.param = iters_so_far + STEPS_BEFORE_WAIT;
  cmd.time = (iters_so_far + STEPS_BEFORE_WAIT) * DUR_PER_STEP;
  cmd.target = source;
  CHECKED(syz_automationBatchAddCommands(batch, 1, &cmd));
  CHECKED(syz_automationBatchExecute(batch));

  syz_handleDecRef(batch);
  return 0;
}

int main(int argc, char *argv[]) {
  struct syz_LibraryConfig library_config;
  syz_Handle context = 0, generator = 0, source = 0, buffer = 0;
  int object_type;
  unsigned int iters_so_far = 0;
  struct syz_Event evt;
  double timebase;

  if (argc != 2) {
    printf("Usage: %s <path>\n", argv[0]);
    return 1;
  }

  syz_libraryConfigSetDefaults(&library_config);
  library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
  library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
  CHECKED(syz_initializeWithConfig(&library_config));

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_setD6(context, SYZ_P_ORIENTATION, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0));

  CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL, NULL));
  CHECKED(syz_createSource3D(&source, context, SYZ_PANNER_STRATEGY_HRTF, 0.0, 1.0, 0.0, NULL, NULL, NULL));
  CHECKED(syz_sourceAddGenerator(source, generator));

  CHECKED(syz_createBufferFromFile(&buffer, argv[1], NULL, NULL));
  CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));
  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));

  CHECKED(syz_contextEnableEvents(context));

  // Grab a time to start automating at.
  CHECKED(syz_getD(&timebase, generator, SYZ_P_SUGGESTED_AUTOMATION_TIME));
  for (;;) {
    printf("Beginning iteration %u\n", iters_so_far);
    CHECKED(enqueueAutomation(context, source, timebase, iters_so_far));

    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      CHECKED(syz_contextGetNextEvent(&evt, context, 0));
    } while (evt.type != SYZ_EVENT_TYPE_USER_AUTOMATION);

    iters_so_far = evt.payload.user_automation.param;
    syz_eventDeinit(&evt);
  }

  syz_handleDecRef(context);
  syz_handleDecRef(generator);
  syz_handleDecRef(buffer);
  syz_handleDecRef(source);
  syz_shutdown();

  return 0;
}
