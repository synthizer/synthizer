#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "synthizer/random_generator.hpp"

int main() {
  synthizer::RandomGenerator gen{};

  printf("Testing generation of single floats\n");
  float fmin = std::numeric_limits<float>::infinity(), fmax = -std::numeric_limits<float>::infinity();
  for (unsigned int i = 0; i < 100000000; i++) {
    float v = gen.generateFloat();
    fmin = std::min(fmin, v);
    fmax = std::max(fmax, v);
  }
  printf("Single floats fmin=%f fmax=%f\n", fmin, fmax);

  fmin = std::numeric_limits<float>::infinity();
  fmax = -std::numeric_limits<float>::infinity();
  printf("Testing float4 generation\n");
  for (unsigned int i = 0; i < 100000000; i++) {
    float f1, f2, f3, f4;
    gen.generateFloat4(f1, f2, f3, f4);
    fmin = std::min(fmin, f1);
    fmin = std::min(fmin, f2);
    fmin = std::min(fmin, f3);
    fmin = std::min(f4, fmin);
    fmax = std::max(fmax, f1);
    fmax = std::max(fmax, f2);
    fmax = std::max(fmax, f3);
    fmax = std::max(fmax, f4);
  }
  printf("float4: fmin = %f fmax = %f\n", fmin, fmax);

  return 0;
}