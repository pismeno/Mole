#include "rendering/vulkan/vulkan_memory_allocator.hpp"

#include <cstring>
#include <memory>
#include <utility>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/vulkan_context.hpp"
#include "utils/result.hpp"

namespace mole {

Result<std::unique_ptr<VulkanMemoryAllocator>> VulkanMemoryAllocator::create(VulkanContext& vk_context) {
  std::unique_ptr<VulkanMemoryAllocator> allocator(new VulkanMemoryAllocator());

  auto r_create_vma = allocator->create_vma(vk_context);
  if (!r_create_vma) return mole::fail(r_create_vma.error());

  return std::move(allocator);
}

Result<void> VulkanMemoryAllocator::create_vma(VulkanContext& vk_context) {
  vma::AllocatorCreateInfo create_info{
      .physicalDevice = *vk_context.get_physical_device(),
      .preferredLargeHeapBlockSize = 0,

      .vulkanApiVersion = VK_API_VERSION_1_3
  };

  auto vkr_create_allocator = vma::raii::createAllocator(vk_context.get_instance(), vk_context.get_logical_device(), create_info);
  if (vkr_create_allocator.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create AMD VMA allocator");
  }

  vma_allocator = std::move(vkr_create_allocator.value);

  return mole::ok();
}

Result<vma::raii::Buffer> VulkanMemoryAllocator::create_buffer(vk::BufferUsageFlags usage, vk::DeviceSize size, const void* data) const {
  vk::BufferCreateInfo buffer_info{
      .size        = size,
      .usage       = usage,
      .sharingMode = vk::SharingMode::eExclusive
  };

  // Currently defaults to Host-Visible, sequentially writable memory
  vma::AllocationCreateInfo alloc_info{
      .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
               vma::AllocationCreateFlagBits::eMapped,
      .usage = vma::MemoryUsage::eAuto
  };

  auto vkr_create_buffer = vma_allocator.createBuffer(buffer_info, alloc_info);
  if (vkr_create_buffer.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create and allocate Buffer via VMA-Hpp");
  }

  vma::raii::Buffer buffer = std::move(vkr_create_buffer.value);

  // If data was provided, copy it immediately since the buffer is already mapped
  if (data != nullptr) {
    const vma::raii::Allocation& allocation = buffer.getAllocation();
    void* mapped_data = allocation.getInfo().pMappedData;
    std::memcpy(mapped_data, data, static_cast<size_t>(size));
  }

  return std::move(buffer);
}

} // mole
