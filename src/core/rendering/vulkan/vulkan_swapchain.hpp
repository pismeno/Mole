#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "utils/result.hpp"

namespace mole {

class VulkanContext;

class VulkanSwapchain {
 public:
  ~VulkanSwapchain() = default;

  inline const vk::Extent2D& get_extent() const { return swap_chain_extent; }
  inline const vk::SurfaceFormatKHR& get_surface_format() const { return swap_chain_surface_format; }
  inline const std::vector<vk::raii::ImageView>& get_image_views() const { return swap_chain_image_views; }
  inline const std::vector<vk::Image>& get_images() const { return swap_chain_images; }
  inline size_t get_image_count() const { return swap_chain_images.size(); }

  Result<uint32_t> acquire_next_image(const vk::raii::Semaphore& present_complete_semaphore);
  Result<void> present(const vk::raii::Queue& present_queue, const vk::raii::Semaphore& render_finished_semaphore, uint32_t image_index);

  Result<void> recreate(VulkanContext& vk_context, uint32_t window_width, uint32_t window_height);

  static Result<std::unique_ptr<VulkanSwapchain>> create(VulkanContext& vk_context, uint32_t window_width, uint32_t window_height);
 private:
  explicit VulkanSwapchain() = default;

  Result<void> create_swap_chain(VulkanContext& vk_context, uint32_t window_width, uint32_t window_height);
  Result<void> create_image_views(VulkanContext& vk_context);

  vk::raii::SwapchainKHR swap_chain = nullptr;

  std::vector<vk::Image> swap_chain_images;
  vk::SurfaceFormatKHR   swap_chain_surface_format;
  vk::Extent2D           swap_chain_extent;

  std::vector<vk::raii::ImageView> swap_chain_image_views;
};

} // mole
