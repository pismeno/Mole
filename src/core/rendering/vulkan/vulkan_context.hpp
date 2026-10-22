#pragma once

#include <cstdint>
#include <memory>

#include <vulkan/vulkan_raii.hpp>

#include "utils/result.hpp"

namespace mole {

class VulkanContext {
 public:
  static constexpr uint32_t PREFERRED_MAX_TEXTURES = 65536;
  static constexpr uint32_t MIN_REQUIRED_TEXTURES  = 1024;

  ~VulkanContext() = default;

  const inline vk::raii::Instance& get_instance() const { return vk_instance; }
  const inline vk::raii::PhysicalDevice& get_physical_device() const { return physical_device; }
  const inline vk::raii::Device& get_logical_device() const { return logical_device; }
  const inline vk::raii::Queue& get_graphics_queue() const { return graphics_queue; }
  const inline uint32_t get_graphics_queue_index() const { return queue_index; }
  const inline vk::raii::SurfaceKHR& get_surface() const { return surface; }
  const inline uint32_t get_texture_capacity() const { return texture_capacity; }

  static Result<std::unique_ptr<VulkanContext>> create(void* native_window_handle);
 private:
  explicit VulkanContext() = default;

  Result<void> create_instance();
  Result<void> create_surface(void* native_window_handle);
  Result<void> pick_physical_device();
  Result<void> create_logical_device();

  [[nodiscard]] bool is_physical_device_suitable(const vk::PhysicalDevice& physical_device) const;

  vk::raii::Context context;

  vk::raii::Instance vk_instance = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::PhysicalDevice physical_device = nullptr;
  vk::raii::Device logical_device = nullptr;
  uint32_t queue_index = ~0;
  vk::raii::Queue graphics_queue = nullptr;

  uint32_t texture_capacity;
};

} // mole
