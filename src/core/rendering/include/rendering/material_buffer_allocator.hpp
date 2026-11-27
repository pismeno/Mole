#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "utils/result.hpp"

namespace mole {

/**
 * @brief CPU-side byte-range allocator for the material SSBO. Every allocation's offset is a multiple of its size,
 * so `offset / size` is an exact array index for a shader that views the buffer as `Material materials[]`.
 */
class MaterialBufferAllocator {
public:
  explicit MaterialBufferAllocator(size_t capacity_bytes);

  /**
   * @brief Allocates `size` bytes and returns the byte offset (a multiple of `size`). Reuses a freed range of the same
   * size if possible. Fails when `size` is 0, not a multiple of 4, or the buffer is full.
   */
  [[nodiscard]] Result<uint32_t> allocate(uint32_t size);

  /** @brief Returns a range previously returned by allocate() with the same size. */
  void free(uint32_t offset, uint32_t size);

  /** @brief Total buffer size in bytes. */
  [[nodiscard]] size_t get_capacity() const;

private:
  size_t capacity_bytes;
  size_t bump_offset = 0;
  std::unordered_map<uint32_t, std::vector<uint32_t>> free_offsets;
};

} // mole
