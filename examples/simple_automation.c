/**
 * Demonstrate simple use of the automation API by synthesizing a tone and playing beeps using an ADSR envelope.
 * */
#include "example_common.h"

#include "synthizer.h"
#include "synthizer_constants.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned int SR = 9000;
static const double FREQ = 100;
static const unsigned int HARMONICS = 10;
static const double PI = 3.141592653589793;

static const struct {

  int interpolation_type;
  double time;
  double value;
} POINTS[] = {
    {SYZ_INTERPOLATION_TYPE_LINEAR, 0.0, 0.0}, {SYZ_INTERPOLATION_TYPE_LINEAR, 0.01, 1.0},
    {SYZ_INTERPOLATION_TYPE_LINEAR, 0.4, 0.5}, {SYZ_INTERPOLATION_TYPE_LINEAR, 0.9, 0.1},
    {SYZ_INTERPOLATION_TYPE_LINEAR, 2.0, 0.0},
};

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

syz_ErrorCode createBatch(syz_Handle *out, syz_Handle context, syz_Handle target, double timebase) {
  unsigned int count = sizeof(POINTS) / sizeof(POINTS[0]);
  struct syz_AutomationCommand *cmds = calloc(count, sizeof(struct syz_AutomationCommand));
  for (unsigned int i = 0; i < count; i++) {
    struct syz_AutomationCommand *c = cmds + i;
    c->type = SYZ_AUTOMATION_COMMAND_APPEND_PROPERTY;
    c->target = target;
    c->params.append_to_property.point.values[0] = POINTS[i].value;
    c->params.append_to_property.point.interpolation_type = POINTS[i].interpolation_type;
    c->params.append_to_property.property = SYZ_P_GAIN;
    c->time = timebase + POINTS[i].time;
  }
  syz_ErrorCode ret = syz_createAutomationBatch(out, context, NULL, NULL);
  if (ret != 0) {
    goto end;
  }
  ret = syz_automationBatchAddCommands(*out, count, cmds);

end:
  free(cmds);
  return ret;
}

int main() {
  float *triangle = NULL;
  syz_Handle context = 0, buffer = 0, generator = 0, source = 0, batch = 0;
  double timebase;

  CHECKED(syz_initialize());

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL, NULL));
  CHECKED(syz_createDirectSource(&source, context, NULL, NULL, NULL));

  triangle = computeTriangle();
  CHECKED(syz_createBufferFromFloatArray(&buffer, SR, 1, SR, triangle, NULL, NULL));
  free(triangle);
  triangle = NULL;

  CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
  CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));

  CHECKED(syz_getD(&timebase, generator, SYZ_P_SUGGESTED_AUTOMATION_TIME));
  CHECKED(createBatch(&batch, context, generator, timebase));
  CHECKED(syz_automationBatchExecute(batch));

  CHECKED(syz_sourceAddGenerator(source, generator));

  printf("Press enter key to clear automation\n");
  getchar();

  syz_handleDecRef(batch);
  batch = 0;
  CHECKED(syz_createAutomationBatch(&batch, context, NULL, NULL));
  CHECKED(syz_automationBatchAddCommands(batch, 1,
                                         &(struct syz_AutomationCommand){
                                             .type = SYZ_AUTOMATION_COMMAND_CLEAR_ALL_PROPERTIES,
                                             .target = generator,
                                         }));
  CHECKED(syz_automationBatchExecute(batch));

  printf("Press enter key to exit\n");
  getchar();

  syz_handleDecRef(source);
  syz_handleDecRef(buffer);
  syz_handleDecRef(batch);
  syz_handleDecRef(generator);
  syz_handleDecRef(context);
  syz_shutdown();

  free(triangle);
  return 0;
}
