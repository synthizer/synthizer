/**
 * Generate a wave and use automation to play it every time the user presses enter.  Demonstrates use of the automation
 * time properties.
 * */
#include "example_common.h"

#include "synthizer.h"
#include "synthizer_constants.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static const unsigned int SR = 9000;
static const double FREQ = 100;
static const unsigned int HARMONICS = 10;
static const double PI = 3.141592653589793;

float *computeTriangle() {
  float *ret = calloc(SR, sizeof(float));
  assert(ret != NULL);
  for (unsigned int i = 0; i < SR; i++) {
    double acc = 0.0;
    for (unsigned int h = 0; h < HARMONICS; h++) {
      unsigned int n = h + 1;
      double hmul = 1.0 / (n * n);
      acc += hmul * sin(2 * PI * n * FREQ * i / (double)SR);
    }
    acc *= 8 / (PI * PI);
    ret[i] = acc;
  }
  return ret;
}

static void automateEnvelope(syz_Handle context, syz_Handle target, double time) {
#define POINT(T, V)                                                                                                    \
  {                                                                                                                    \
    .target = target, .type = SYZ_AUTOMATION_COMMAND_APPEND_PROPERTY, .time = (T), .params.append_to_property = {      \
      .property = SYZ_P_GAIN,                                                                                          \
      .point = {.interpolation_type = SYZ_INTERPOLATION_TYPE_LINEAR, .values = {(V)}}                                  \
    }                                                                                                                  \
  }
  struct syz_AutomationCommand commands[] = {
      {.target = target, .type = SYZ_AUTOMATION_COMMAND_CLEAR_ALL_PROPERTIES},
      POINT(time, 0.0),
      POINT(time + 0.005, 1.0),
      POINT(time + 0.010, 0.7),
      POINT(time + 0.20, 0.1),
      POINT(time + 0.21, 0.0),
  };

  syz_Handle batch;

  CHECKED(syz_createAutomationBatch(&batch, context, NULL, NULL));
  CHECKED(syz_automationBatchAddCommands(batch, sizeof(commands) / sizeof(commands[0]), commands));
  CHECKED(syz_automationBatchExecute(batch));
  syz_handleDecRef(batch);
}

int main() {
  syz_Handle context, buffer, generator, source;
  float *wave;

  CHECKED(syz_initialize());

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_createDirectSource(&source, context, NULL, NULL, NULL));
  CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL, NULL));

  wave = computeTriangle();
  CHECKED(syz_createBufferFromFloatArray(&buffer, SR, 1, SR, wave, NULL, NULL));
  free(wave);

  CHECKED(syz_setD(generator, SYZ_P_GAIN, 0.0));
  CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));
  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
  CHECKED(syz_sourceAddGenerator(source, generator));

  printf("Press enter to play notes...");
  while (1) {
    double time;

    getchar();
    CHECKED(syz_getD(&time, generator, SYZ_P_SUGGESTED_AUTOMATION_TIME));
    automateEnvelope(context, generator, time);
  }

  syz_handleDecRef(source);
  syz_handleDecRef(generator);
  syz_handleDecRef(buffer);
  syz_handleDecRef(context);

  return 0;
}
