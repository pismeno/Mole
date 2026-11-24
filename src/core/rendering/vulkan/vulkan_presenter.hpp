#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/vulkan_context.hpp"
#include "utils/result.hpp"

namespace mole {

class IWindow;
class VulkanSwapchain;

/**
 * @brief Private helper of VulkanRenderer: a window surface, its swapchain and per-frame/per-image sync objects.
 * Recreates its swapchain on acquire when the surface extent changed or a previous acquire/present reported it stale.
 */
class VulkanPresenter {
public:
  ~VulkanPresenter();

  /**
   * @brief Acquires the next backbuffer for `frame_index` unless already acquired this frame.
   * @return ok(true) when acquired, ok(false) when the surface can't be rendered right now (zero extent, or still out
   * of date after one recreate-and-retry), fail on fatal errors.
   */
  Result<bool> acquire(uint32_t frame_index);

  /** @brief Clears the acquired state after the frame was presented. */
  void release();

  /** @brief Requests a swapchain recreation on the next acquire (after an out-of-date/suboptimal present). */
  void mark_needs_recreate();

  [[nodiscard]] bool is_acquired() const;
  [[nodiscard]] uint32_t get_acquired_image_index() const;
  [[nodiscard]] vk::Image get_acquired_image() const;
  [[nodiscard]] vk::ImageView get_acquired_image_view() const;
  [[nodiscard]] vk::ImageLayout& get_acquired_image_layout();
  [[nodiscard]] const vk::raii::Semaphore& get_image_available(uint32_t frame_index) const;
  [[nodiscard]] const vk::raii::Semaphore& get_render_finished() const;

  [[nodiscard]] vk::SwapchainKHR get_swapchain() const;
  [[nodiscard]] vk::Extent2D get_extent() const;
  [[nodiscard]] vk::Format get_format() const;
  [[nodiscard]] uint32_t get_image_count() const;

  static Result<std::unique_ptr<VulkanPresenter>> create(const VulkanContext& vk_context, const IWindow* window);

private:
  explicit VulkanPresenter(const VulkanContext& vk_context, const IWindow* window);

  Result<void> create_surface();
  Result<void> create_sync_objects();
  Result<void> create_render_finished_semaphores();

  /// Makes sure a swapchain matching the surface exists. ok(false) means the surface has zero extent.
  Result<bool> ensure_swapchain();

  // Declaration order matters: the surface must be destroyed after the swapchain.
  vk::raii::SurfaceKHR surface = nullptr;
  std::unique_ptr<VulkanSwapchain> swapchain;
  std::array<vk::raii::Semaphore, VulkanContext::MAX_FRAMES_IN_FLIGHT> image_available{
    nullptr,
    nullptr
  };
  std::vector<vk::raii::Semaphore> render_finished;

  const VulkanContext* vk_context;
  const IWindow* window;

  uint32_t acquired_image_index = 0;
  vk::ImageLayout acquired_image_layout = vk::ImageLayout::eUndefined;
  bool acquired = false;
  bool needs_recreate = false;
};

} // mole
