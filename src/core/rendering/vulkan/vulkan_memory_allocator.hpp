#pragma once

#include <memory>
#include <span>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "utils/result.hpp"

namespace mole {

class VulkanContext;

class VulkanMemoryAllocator {
 public:
  ~VulkanMemoryAllocator() = default;

  Result<vma::raii::Buffer> create_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, const void* data = nullptr) const;

  template <typename T>
  Result<vma::raii::Buffer> create_buffer(vk::BufferUsageFlags usage, std::span<T> data) const {
    return create_buffer(usage, data.size_bytes(), data.data());
  }

  inline const vma::raii::Allocator& get_allocator() const { return vma_allocator; }

  static Result<std::unique_ptr<VulkanMemoryAllocator>> create(VulkanContext& vk_context);
 private:
  explicit VulkanMemoryAllocator() = default;

  Result<void> create_vma(VulkanContext& vk_context);

  vma::raii::Allocator vma_allocator = nullptr;
};

} // mole
