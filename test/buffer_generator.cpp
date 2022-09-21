#include "synthizer/config.hpp"
#include "synthizer/generators/buffer.hpp"

#include <catch2/catch_all.hpp>

#include <cstdint>

using namespace synthizer;

/**
 * Simulate a read over a buffer, checking that nothing ever tries to read past the end.
 *
 * length and delta should be scaled by config::BUFFER_POS_MULTIPLIER
 *
 * We don't test the looping case because it is difficult to do so and is much more straightforward anyway: none of the
 * special case machinery ends up applying.
 * */
static void simulateBuffer(std::uint64_t length, std::uint64_t delta) {
  std::uint64_t iterations = 0, full_block_iterations = 0;
  std::uint64_t scaled_position = 0;

  while (true) {
    auto params = buffer_generator_detail::computePitchBendParams(scaled_position, delta, length, false);

    CAPTURE(length, delta, iterations, full_block_iterations, scaled_position, params.include_implicit_zero,
            params.span_start, params.span_len, params.iterations, params.offset);
    if (params.iterations == 0) {
      break;
    }
    if (params.iterations == config::BLOCK_SIZE) {
      full_block_iterations += 1;
    }

    REQUIRE(scaled_position - params.span_start * config::BUFFER_POS_MULTIPLIER < config::BUFFER_POS_MULTIPLIER);
    // Compute the upper sample in this simulated iteration, and make sure it is within bounds of the buffer.
    std::uint64_t upper =
        (params.span_start * config::BUFFER_POS_MULTIPLIER + params.offset + (params.iterations - 1) * delta) /
        config::BUFFER_POS_MULTIPLIER + 1;
    REQUIRE(upper < length / config::BUFFER_POS_MULTIPLIER + 1);

    // the returned span should also be in bounds of the buffer, except that it may include the implicit zero.  If it
    // includes the implicit zero, we get one more.
    std::uint64_t max_len =
        (length + config::BUFFER_POS_MULTIPLIER * params.include_implicit_zero) / config::BUFFER_POS_MULTIPLIER;
    REQUIRE(params.span_start + params.span_len <= max_len);

    // If we are being told to do some work, we must always have two samples.
    REQUIRE(params.span_len >= 2);

    // The distance of the upper index from the start of the span must be less than the length.
    REQUIRE(upper >= params.span_start);
    REQUIRE(upper - params.span_start < params.span_len);

    scaled_position += params.iterations * delta;
    ++iterations;
  }

  if (delta > 0) {
    REQUIRE(iterations > 0);
    REQUIRE((iterations == full_block_iterations + 1) | (iterations == full_block_iterations));
  }
}

TEST_CASE("BufferGenerator: pitch bend computation") {
  // use an increment that's unusual enough to try to hit all the edge cases without having to search all
  // combinations, which is slow.
  //
  // If there is elieved to be a bug, these can be turned up.
  for (std::uint64_t delta = 256; delta < 2 * config::BUFFER_POS_MULTIPLIER; delta += 31) {
    for (std::uint64_t len = 1; len < 2048; len += 111) {
      CAPTURE(len, delta);
      simulateBuffer(len * config::BUFFER_POS_MULTIPLIER, delta);
    }
  }
}
