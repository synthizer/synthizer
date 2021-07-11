#include "synthizer/compiler_specifics.hpp"
#include "synthizer/config.hpp"
#include "synthizer/filter_design.hpp"
#include "synthizer/noise_generator.hpp"
#include "synthizer/random_generator.hpp"

#include <cassert>

namespace synthizer {

NoiseGenerator::NoiseGenerator() {
	/*
	 * Butterworth with cutoff of 500hz at 44100 sr designed with Scipy.
	 * TODO: implement butterworth sections ourselves and get rid of this.
	 * */
	auto def = combineIIRFilters(designOneZero(-1), designOnePole(0.96));
	this->brown_filter.setParameters(def);
}

int NoiseGenerator::getNoiseType() {
	return this->noise_type;
}

void NoiseGenerator::setNoiseType(int type) {
	this->noise_type = type;
	if (this->noise_type == SYZ_NOISE_TYPE_VM) {
		this->initVM();
	} else if (this->noise_type == SYZ_NOISE_TYPE_FILTERED_BROWN) {
		this->initFilteredBrown();
	}
}

float NoiseGenerator::generateSample() {
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

void NoiseGenerator::generateBlock(unsigned int size, float *block, unsigned int stride) {
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

float NoiseGenerator::generateSampleUniform() {
	return this->random_gen.generateFloat();
}

void NoiseGenerator::generateBlockUniform(unsigned int size, float *block, unsigned int stride) {
	unsigned int i = 0;
	for (unsigned int j = 0; j < size / 4; i+=4, j++) {
		float f1, f2, f3, f4;
		this->random_gen.generateFloat4(f1, f2, f3, f4);
		block[i * stride] += f1;
		block[(i+1) * stride] += f2;
		block[(i+2) * stride] += f3;
		block[(i+3) * stride] += f4;
	}
	for (; i < size; i++) {
		block[i * stride] += this->random_gen.generateFloat();
	}
}

void NoiseGenerator::initVM() {
	this->vm_accumulator = 0.0f;
	for (auto &x: this->vm_values) {
		x = this->random_gen.generateFloat();
		this->vm_accumulator += x;
	}
}

float NoiseGenerator::generateSampleVM() {
	int index = __builtin_ctz(this->vm_counter | VM_MASK);
	this->vm_counter ++;
	this->vm_accumulator -= this->vm_values[index];
	this->vm_values[index] = this->random_gen.generateFloat();
	this->vm_accumulator += this->vm_values[index];
	float out = this->vm_accumulator + this->random_gen.generateFloat();
	return out * (1.0f / VM_GENERATOR_COUNT);
}

void NoiseGenerator::generateBlockVM(unsigned int size, float *block, unsigned int stride) {
	for (unsigned int i = 0; i < size; i++) {
		block[i * stride] += this->generateSampleVM();
	}
}

void NoiseGenerator::initFilteredBrown() {
	this->brown_filter.reset();
}

float NoiseGenerator::generateSampleFilteredBrown() {
	float sample = generateSampleUniform();
	this->brown_filter.tick(&sample, &sample);
	return sample;
}

void NoiseGenerator::generateBlockFilteredBrown(unsigned int size, float *block, unsigned int stride) {
	unsigned int i =0;
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

}
