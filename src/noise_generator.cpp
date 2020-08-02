#include "synthizer/noise_generator.hpp"
#include "synthizer/random_generator.hpp"

#include <cassert>

namespace synthizer {

int NoiseGenerator::getNoiseType() {
	return this->noise_type;
}

void NoiseGenerator::setNoiseType(int type) {
	this->noise_type = type;
	if (this->noise_type == SYZ_NOISE_TYPE_VM) {
		this->initVM();
	}
}

float NoiseGenerator::generateSample() {
	switch (this->noise_type) {
	case SYZ_NOISE_TYPE_UNIFORM:
		return this->generateSampleUniform();
	case SYZ_NOISE_TYPE_VM:
		return this->generateSampleVM();
	default:
		assert(nullptr == "This should be unreachable");
	}
	return 0.0f;
}

void NoiseGenerator::generateBlock(unsigned int size, AudioSample *block, unsigned int stride) {
	switch (this->noise_type) {
	case SYZ_NOISE_TYPE_UNIFORM:
		return this->generateBlockUniform(size, block, stride);
	case SYZ_NOISE_TYPE_VM:
		return this->generateBlockVM(size, block, stride);
	default:
		assert(nullptr == "This should be unreachable");
	}
}

float NoiseGenerator::generateSampleUniform() {
	return this->random_gen.generateFloat();
}

void NoiseGenerator::generateBlockUniform(unsigned int size, AudioSample *block, unsigned int stride) {
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

void NoiseGenerator::generateBlockVM(unsigned int size, AudioSample *block, unsigned int stride) {
	for (unsigned int i = 0; i < size; i++) {
		block[i * stride] += this->generateSampleVM();
	}
}

}