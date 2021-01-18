/**
 * Try to detect races in GenerationThread by writing increasing floats and seeing if
 * it falls over (and if so, when).
 * */
#include "synthizer/config.hpp"
#include "synthizer/generation_thread.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

using namespace synthizer;

int main() {
	std::array<float, config::BLOCK_SIZE> tmp_buf;
	GenerationThread gt{1, config::BLOCK_SIZE * 10};
	float gen_float = 0.0f, expected_float = 0.0f;
	std::size_t read_so_far = 0;
	const std::size_t iterations = 30000;

	gt.start([&](unsigned int ch, float *out) {
		for (unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
		 out[i] = gen_float;
		 gen_float++;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	});

	while (read_so_far < iterations) {
		std::size_t got = gt.read(std::min<std::size_t>(config::BLOCK_SIZE, (iterations - read_so_far)), &tmp_buf[0]);
		for (std::size_t i = 0; i < got; i++) {
			if (tmp_buf[i] != expected_float) {
				std::printf("%f != %f", tmp_buf[i], expected_float);
				return 1;
			}
			expected_float++;
		}
		read_so_far += got;
	}


	return 0;
}
