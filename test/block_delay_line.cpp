#include "synthizer/block_delay_line.hpp"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <variant>

using namespace synthizer;

TEST_CASE("BlockDelayLine ModPointer interface") {
  constexpr std::size_t BLOCKS = 4;
  constexpr std::size_t FRAMES = BLOCKS * config::BLOCK_SIZE;
  constexpr std::size_t SAMPLES = FRAMES * 2;

  auto line = BlockDelayLine<2, 4>{};

  // use the fact that we know the internals to fill the line with incrementing floats.
  auto input_b = line.getNextBlock();
  for (std::size_t i = 0; i < SAMPLES; i++) {
    input_b[i] = i;
  }

  // The line is now sitting back at the beginning. Let's ask it for something that can read 5 frames into the past
  // repeatedly, and find out if those are what we expect to see.
  for (std::size_t block = 0; block < 10; block++) {
    auto mp = line.getModPointer(5);
    std::visit(
        [&](auto &ptr) {
          for (std::size_t i = 0; i < config::BLOCK_SIZE; i++) {
            for (std::size_t d = 0; d <= 5; d++) {
              auto ptr2 = ptr - 2 * d;
              std::size_t expected = (SAMPLES + 2 * block * config::BLOCK_SIZE + 2 * i - 2 * d) % SAMPLES;
              REQUIRE(ptr2[0] == expected);
              REQUIRE(ptr2[1] == expected + 1);
            }
            ptr += 2;
          }
        },
        mp);
    line.incrementBlock();
  }
}
