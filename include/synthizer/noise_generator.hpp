#pragma once

#include "synthizer.h"
#include "synthizer_constants.h"

#include "synthizer/compiler_specifics.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/random_generator.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <cstdint>

namespace synthizer {

class NoiseGenerator {
public:
  NoiseGenerator();

  int getNoiseType();
  void setNoiseType(int type);
  float generateSample();
  /* Adds, as with the rest of Synthizer. */
  void generateBlock(unsigned int size, float *block, unsigned int stride = 1);

private:
  float generateSampleUniform();
  void generateBlockUniform(unsigned int size, float *block, unsigned int stride);

  RandomGenerator random_gen;
  int noise_type = SYZ_NOISE_TYPE_UNIFORM;

  /*
   * Voss-mcCartney algorithm to generate pink noise.
   *
   * This consists of some number of stacked random generators, which are set up so that we only update some at a time.
   * I.e. sample 1: 0 sample 2: 1 sample 3: 0 sample 4: 2 sample 5: 0 sample 6: 1
   *
   * And so on. Notice that 0 is updated twice as often as 1,
   * 1 twice as often as 2, and so on.  This effectively gives us decreasing power in the higher octaves, though
   * this algorithm was taken from other people and I don't have a proof that it works to generate pink noise reliably.
   * */
  /* Must never be greater than 32. */
  static const unsigned int VM_GENERATOR_COUNT = 14;
  /*
   * One generator is always run, so we don't do it here.
   * */
  std::array<float, VM_GENERATOR_COUNT - 1> vm_values = {0.0f};
  float vm_accumulator = 0.0f;
  /*
   * Mask to turn the counter into something safe for __builtin_ctz. By oring with the counter,
   * we make sure the high bits are always set, that ctz never gets 0, and that ctz never returns something
   * higher than the maximum array index.
   *
   * -2 because this accounts for the always-run generator.
   * */
  static const unsigned int VM_MASK = ~((1 << (VM_GENERATOR_COUNT - 2u)) - 1u);
  unsigned int vm_counter = 0;
  void initVM();
  float generateSampleVM();
  void generateBlockVM(unsigned int size, float *block, unsigned int stride);

  /* -6DB filter for brown noise. */
  IIRFilter<1, 3, 3> brown_filter;
  void initFilteredBrown();
  float generateSampleFilteredBrown();
  void generateBlockFilteredBrown(unsigned int size, float *block, unsigned int stride);
};

inline NoiseGenerator::NoiseGenerator() {
  /*
   * Butterworth with cutoff of 500hz at 44100 sr designed with Scipy.
   * TODO: implement butterworth sections ourselves and get rid of this.
   * */
  auto def = combineIIRFilters(designOneZero(-1), designOnePole(0.96));
  this->brown_filter.setParameters(def);
}

inline int NoiseGenerator::getNoiseType() { return this->noise_type; }

inline void NoiseGenerator::setNoiseType(int type) {
  this->noise_type = type;
  if (this->noise_type == SYZ_NOISE_TYPE_VM) {
    this->initVM();
  } else if (this->noise_type == SYZ_NOISE_TYPE_FILTERED_BROWN) {
    this->initFilteredBrown();
  }
}

inline float NoiseGenerator::generateSample() {
  switch (this->noise_type) {
  case SYZ_NOISE_TYPE_UNIFORM:
    return this->generateSampleUniform();
  case SYZ_NOISE_TYPE_VM:
    return this->generateSampleVM();
  case SYZ_NOISE_TYPE_FILTERED_BROWN:
    return this->generateSampleFilteredBrown();
  default:
    assert("This should be unreachable");
  }
  return 0.0f;
}

inline void NoiseGenerator::generateBlock(unsigned int size, float *block, unsigned int stride) {
  switch (this->noise_type) {
  case SYZ_NOISE_TYPE_UNIFORM:
    return this->generateBlockUniform(size, block, stride);
  case SYZ_NOISE_TYPE_VM:
    return this->generateBlockVM(size, block, stride);
  case SYZ_NOISE_TYPE_FILTERED_BROWN:
    return this->generateBlockFilteredBrown(size, block, stride);
  default:
    assert("This should be unreachable");
  }
}

inline float NoiseGenerator::generateSampleUniform() { return this->random_gen.generateFloat(); }

inline void NoiseGenerator::generateBlockUniform(unsigned int size, float *block, unsigned int stride) {
  unsigned int i = 0;
  for (unsigned int j = 0; j < size / 4; i += 4, j++) {
    float f1, f2, f3, f4;
    this->random_gen.generateFloat4(f1, f2, f3, f4);
    block[i * stride] += f1;
    block[(i + 1) * stride] += f2;
    block[(i + 2) * stride] += f3;
    block[(i + 3) * stride] += f4;
  }
  for (; i < size; i++) {
    block[i * stride] += this->random_gen.generateFloat();
  }
}

inline void NoiseGenerator::initVM() {
  this->vm_accumulator = 0.0f;
  for (auto &x : this->vm_values) {
    x = this->random_gen.generateFloat();
    this->vm_accumulator += x;
  }
}

inline float NoiseGenerator::generateSampleVM() {
  int index = __builtin_ctz(this->vm_counter | VM_MASK);
  this->vm_counter++;
  this->vm_accumulator -= this->vm_values[index];
  this->vm_values[index] = this->random_gen.generateFloat();
  this->vm_accumulator += this->vm_values[index];
  float out = this->vm_accumulator + this->random_gen.generateFloat();
  return out * (1.0f / VM_GENERATOR_COUNT);
}

inline void NoiseGenerator::generateBlockVM(unsigned int size, float *block, unsigned int stride) {
  for (unsigned int i = 0; i < size; i++) {
    block[i * stride] += this->generateSampleVM();
  }
}

inline void NoiseGenerator::initFilteredBrown() { this->brown_filter.reset(); }

inline float NoiseGenerator::generateSampleFilteredBrown() {
  float sample = generateSampleUniform();
  this->brown_filter.tick(&sample, &sample);
  return sample;
}

inline void NoiseGenerator::generateBlockFilteredBrown(unsigned int size, float *block, unsigned int stride) {
  unsigned int i = 0;
  /*for (unsigned int j = 0; j < size / 4; i += 4, j++) {
          float f1, f2, f3, f4;
          this->random_gen.generateFloat4(f1, f2, f3, f4);
          this->brown_filter.tick(&f1, block + i * stride);
          this->brown_filter.tick(&f2, block + (i + 1) * stride);
          this->brown_filter.tick(&f3, block + (i + 2) * stride);
          this->brown_filter.tick(&f4, block + (i + 3) * stride);
  }*/
  for (; i < size; i++) {
    block[i * stride] = this->generateSampleFilteredBrown();
  }
}

} // namespace synthizer