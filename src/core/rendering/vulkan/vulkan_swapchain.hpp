#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "utils/result.hpp"

namespace mole {

class VulkanContext;

enum class ESwapchainStatus {
  Ok,
  Suboptimal,
  OutOfDate,
  SurfaceLost,
  DeviceLost,
  Error
};

struct SwapchainAcquireResult {
  ESwapchainStatus status;
  uint32_t image_index;
};

/** @brief Maps a present/acquire vk::Result to a swapchain status. */
[[nodiscard]] ESwapchainStatus to_swapchain_status(vk::Result result);

class VulkanSwapchain {
public:
  ~VulkanSwapchain() = default;

  inline const vk::Extent2D& get_extent() const { return swap_chain_extent; }
  inline const vk::SurfaceFormatKHR& get_surface_format() const { return swap_chain_surface_format; }
  inline const std::vector<vk::raii::ImageView>& get_image_views() const { return swap_chain_image_views; }
  inline const std::vector<vk::Image>& get_images() const { return swap_chain_images; }
  inline size_t get_image_count() const { return swap_chain_images.size(); }
  [[nodiscard]] const vk::raii::SwapchainKHR& get_swapchain() const;

  [[nodiscard]] SwapchainAcquireResult acquire_next_image(const vk::raii::Semaphore& image_available_semaphore);

  Result<void> recreate(const VulkanContext& vk_context, const vk::raii::SurfaceKHR& surface, vk::Extent2D extent);

  static Result<std::unique_ptr<VulkanSwapchain>> create(const VulkanContext& vk_context,
                                                         const vk::raii::SurfaceKHR& surface, vk::Extent2D extent);
private:
  explicit VulkanSwapchain() = default;

  Result<void> create_swap_chain(const VulkanContext& vk_context, const vk::raii::SurfaceKHR& surface,
                                 vk::Extent2D extent);
  Result<void> create_image_views(const VulkanContext& vk_context);

  vk::raii::SwapchainKHR swap_chain = nullptr;

  std::vector<vk::Image> swap_chain_images;
  vk::SurfaceFormatKHR swap_chain_surface_format;
  vk::Extent2D swap_chain_extent;

  std::vector<vk::raii::ImageView> swap_chain_image_views;
};

} // mole
