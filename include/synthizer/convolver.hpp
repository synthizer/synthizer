#pragma once

#include <array>
#include <algorithm>
#include <cassert>
#include <type_traits>

#include "synthizer/types.hpp"

namespace synthizer {

/*
 * A convolver, for when all the sizes are known.
 *  csize is the impulse response size. bsize is the block size.
 * 
 * This library tries to use this convolver whenever possible since knowing the sizes at compile time is highly beneficial. It's templated to allow for vector types.
 * 
 * input and output should not overlap. Outputs BSIZE samples. Assumes that input is BSIZE+CSIZE-1 elements.
 * 
 * If add is true, we add to the output instead of clearing it.
 * 
 * Stride applies to all arguments.
 */
template<typename T, unsigned int bsize, unsigned int csize, bool add=false>
void constantSizeConvolver(const T *input, T *output, const  T *response, unsigned int stride = 1) {
	for(unsigned int i = 0; i < bsize; i++) {

		T result = zeroValue<T>();
		if(add == false)
			output[i] = zeroValue<T>();
		for(unsigned int j = 0; j < csize; j++) {
			result += input[i+j]*response[csize-j-1];
		}
		if(add == false)
			output[i] = result;
		else
			output[i] += result;
	}
}

/*
 * Does the heavy lifting of being a multi-lane convolver.
 * 
 * In future, this will be specialized via compiler magic to use SIMD, so all parameters need to be multiples of 4, and lanes a power of 2.
 */
template<unsigned int block_size, unsigned int impulse_size, unsigned int lanes, bool add=false>
auto constantSizeMultilaneConvolver(AudioSample *input, AudioSample *output, AudioSample *responses)
-> All<void, PowerOfTwo<lanes>, MultipleOf<lanes, 4>, MultipleOf<impulse_size, 4>, MultipleOf<block_size, 4>> {
	const auto stride = lanes/4;
	for(unsigned int l = 0; l < lanes/4; l++) {
		constantSizeConvolver<AudioSample4, block_size, impulse_size, add>(
			((AudioSample4*)input)+l, ((AudioSample4*)output)+l, ((AudioSample4*)responses)+l, stride);
	}
}

}
