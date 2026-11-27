#include "rendering/material_buffer_allocator.hpp"

#include <string>

namespace mole {

MaterialBufferAllocator::MaterialBufferAllocator(size_t capacity_bytes) :
  capacity_bytes(capacity_bytes) {}

Result<uint32_t> MaterialBufferAllocator::allocate(uint32_t size) {
  if (size == 0 || size % 4 != 0) {
    return mole::fail("MaterialBufferAllocator: invalid allocation size " + std::to_string(size));
  }

  auto it = free_offsets.find(size);
  if (it != free_offsets.end() && !it->second.empty()) {
    uint32_t offset = it->second.back();
    it->second.pop_back();
    return offset;
  }

  size_t offset = ((bump_offset + size - 1) / size) * size;
  if (offset + size > capacity_bytes) {
    return mole::fail("MaterialBufferAllocator: out of space for " + std::to_string(size) + " bytes");
  }

  bump_offset = offset + size;
  return static_cast<uint32_t>(offset);
}

void MaterialBufferAllocator::free(uint32_t offset, uint32_t size) { free_offsets[size].push_back(offset); }

size_t MaterialBufferAllocator::get_capacity() const { return capacity_bytes; }

} // mole
