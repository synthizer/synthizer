#include "synthizer/convolver.hpp"
#include "synthizer/types.hpp"
#include <chrono>
#include <stdio.h>

const unsigned int BLOCK_SIZE = 1<<24;
const unsigned int IMPULSE_SIZE = 32;
const unsigned int LANES = 4;
alignas(16) synthizer::AudioSample input[(BLOCK_SIZE+IMPULSE_SIZE)*LANES] = {0 };
alignas(16) synthizer::AudioSample impulse[IMPULSE_SIZE*LANES] = { 0 };
alignas(16) synthizer::AudioSample output[BLOCK_SIZE*LANES] = { 0 };

int main() {
	auto start = std::chrono::high_resolution_clock::now();
	synthizer::constantSizeMultilaneConvolver<BLOCK_SIZE, IMPULSE_SIZE, LANES>(&input[0], &output[0], &impulse[0]);
	auto end = std::chrono::high_resolution_clock::now();
	auto sum = 0.0f;
	for(unsigned int i=0; i < BLOCK_SIZE; i++) sum += output[i];
	std::chrono::duration<double> delta = end-start;
	printf("Sum: %f, time: %f", sum, delta.count());
	while(getchar() != '\n');
	return 0;
}