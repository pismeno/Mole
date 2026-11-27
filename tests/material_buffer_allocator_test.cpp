#include <cstdint>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "rendering/material_buffer_allocator.hpp"

namespace mole {

TEST_CASE("MaterialBufferAllocator offsets are multiples of the size") {
  MaterialBufferAllocator allocator(128 * 1000);
  for (uint32_t size : {4u, 32u, 48u, 80u}) {
    for (int i = 0; i < 5; ++i) {
      auto r = allocator.allocate(size);
      REQUIRE(r.is_ok());
      CHECK(std::move(r).value() % size == 0);
    }
  }
}

TEST_CASE("MaterialBufferAllocator ranges never overlap across mixed sizes") {
  MaterialBufferAllocator allocator(128 * 1000);
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  const uint32_t sizes[] = {4, 48, 32, 80, 4, 112, 48, 16};
  for (int round = 0; round < 10; ++round) {
    for (uint32_t size : sizes) {
      auto r = allocator.allocate(size);
      REQUIRE(r.is_ok());
      ranges.push_back({std::move(r).value(), size});
    }
  }
  for (size_t i = 0; i < ranges.size(); ++i) {
    for (size_t j = i + 1; j < ranges.size(); ++j) {
      auto [a, a_size] = ranges[i];
      auto [b, b_size] = ranges[j];
      CHECK((a + a_size <= b || b + b_size <= a));
    }
  }
}

TEST_CASE("MaterialBufferAllocator reuses freed offsets of the same size") {
  MaterialBufferAllocator allocator(1024);
  auto r_a = allocator.allocate(48);
  REQUIRE(r_a.is_ok());
  uint32_t a = std::move(r_a).value();
  REQUIRE(allocator.allocate(48).is_ok());
  allocator.free(a, 48);
  auto r_c = allocator.allocate(48);
  REQUIRE(r_c.is_ok());
  CHECK(std::move(r_c).value() == a);
}

TEST_CASE("MaterialBufferAllocator fails on invalid size and when full") {
  MaterialBufferAllocator allocator(96);
  CHECK_FALSE(allocator.allocate(0).is_ok());
  CHECK_FALSE(allocator.allocate(6).is_ok());
  REQUIRE(allocator.allocate(48).is_ok());
  REQUIRE(allocator.allocate(48).is_ok());
  CHECK_FALSE(allocator.allocate(48).is_ok());
  CHECK_FALSE(allocator.allocate(4).is_ok());
}

} // mole
