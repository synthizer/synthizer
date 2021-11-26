/**
 * Run through the wave types supported by the fast sine bank generator.
 * */
#include "example_common.h"

#include "synthizer.h"
#include "synthizer_constants.h"

#include <assert.h>
#include <chrono>
#include <math.h>
#include <stdio.h>
#include <thread>

void driveGenerator(syz_Handle g) {
  for (double freq = 300.0; freq > 100.0; freq -= 5.0) {
    CHECKED(syz_setD(g, SYZ_P_FREQUENCY, freq));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
}

int main() {
  syz_Handle context, generator, source;

  CHECKED(syz_initialize());

  CHECKED(syz_createContext(&context, NULL, NULL));
  CHECKED(syz_createDirectSource(&source, context, NULL, NULL, NULL));

  printf("Sine\n");
  CHECKED(syz_createSineBankGeneratorSineWave(&generator, context, 300.0, NULL, NULL, NULL));
  CHECKED(syz_sourceAddGenerator(source, generator));
  driveGenerator(generator);
  syz_handleDecRef(generator);
  generator = 0;

  printf("Square partials=10\n");
  CHECKED(syz_createSineBankGeneratorSquareWave(&generator, context, 220.0, 10, NULL, NULL, NULL));
  CHECKED(syz_sourceAddGenerator(source, generator));
  driveGenerator(generator);
  syz_handleDecRef(generator);
  generator = 0;

  syz_handleDecRef(source);
  syz_handleDecRef(generator);
  syz_handleDecRef(context);
  syz_shutdown();

  return 0;
}
