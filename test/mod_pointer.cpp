#include "synthizer/mod_pointer.hpp"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <variant>
#include <vector>

template <std::size_t LEN>
using TestModSlice = synthizer::ModSlice<int, synthizer::mod_pointer_detail::StaticLengthProvider<LEN>>;

TEST_CASE("ModSlice") {
  std::vector<int> data{};
  constexpr unsigned int NUM_INDICES = 10000;

  for (int i = 0; i < NUM_INDICES; i++) {
    data.push_back(i);
  }

  SECTION("basic indexing") {
    auto ptr = TestModSlice<NUM_INDICES>{data.data(), 5000,
                                         synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};
    REQUIRE(*ptr == 5000);
    REQUIRE(ptr[0] == 5000);
    REQUIRE(ptr[100] == 5100);
    REQUIRE(ptr[10000] == 5000);
    REQUIRE(ptr[9900] == 4900);
  }

  SECTION("Basic incrementing loops") {
    auto ptr =
        TestModSlice<NUM_INDICES>{data.data(), 0, synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};

    for (std::size_t ign = 0; ign < 5; ign++) {
      for (std::size_t i = 0; i < 10000; i++) {
        REQUIRE(*ptr == i);
        REQUIRE(ptr[0] == i);
        REQUIRE(ptr[1] == (i + 1) % NUM_INDICES);
        ptr++;
      }
    }
  }

  SECTION("Operator []") {
    auto ptr = TestModSlice<NUM_INDICES>{data.data(), 5000,
                                         synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};

    for (std::size_t i = 0; i < 10000; i++) {
      REQUIRE(ptr[i] == (i + 5000) % NUM_INDICES);
    }
  }

  SECTION("Make sure we can decrement and subtract") {
    auto ptr =
        TestModSlice<NUM_INDICES>{data.data(), 0, synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};
    auto ptr2 =
        TestModSlice<NUM_INDICES>{data.data(), 0, synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};
    auto ptr3 =
        TestModSlice<NUM_INDICES>{data.data(), 0, synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};

    for (std::size_t i = 0; i < NUM_INDICES; i++) {
      // Check subtracting by an increment.
      REQUIRE(*(ptr - 5) == (NUM_INDICES * 2 - i - 5) % NUM_INDICES);

      ptr--;
      --ptr2;
      ptr3 = ptr3 - 1;
      std::size_t expected = NUM_INDICES - i - 1;
      REQUIRE(*ptr == expected);
      REQUIRE(*ptr2 == expected);
      REQUIRE(*ptr3 == expected);
    }
  }

  SECTION("Make sure we can write") {
    auto ptr =
        TestModSlice<NUM_INDICES>{data.data(), 0, synthizer::mod_pointer_detail::StaticLengthProvider<NUM_INDICES>{}};

    for (std::size_t i = 0; i < NUM_INDICES; i++) {
      ptr[i] += 1;
      REQUIRE(ptr[i] == i + 1);
      REQUIRE(data[i] == i + 1);
      auto ptr2 = ptr + i;
      *ptr2 += 1;
      REQUIRE(*ptr2 == i + 2);
      REQUIRE(data[i] == i + 2);
    }
  }
}

TEST_CASE("ModPointer") {
  int data[5] = {0};

  // table is {offset, max_index, expected_variant_index}
  auto params = GENERATE(Catch::Generators::table<std::size_t, std::size_t, std::size_t>({
      {0, 4, 1},
      {0, 5, 1},
      {1, 3, 1},
      {1, 10, 0},
      {0, 4, 1},
      {4, 1, 1},
  }));

  auto [offset, max_index, variant_index] = params;

  auto ptr = synthizer::createModPointer<int, 5>(data, offset, max_index);
  REQUIRE(ptr.index() == variant_index);
}
