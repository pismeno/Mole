#include "rendering/vulkan/vulkan_presenter.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#ifdef MOLE_PLATFORM_WINDOWS
#include <windows.h>
#endif
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "osal/iwindow.hpp"
#include "rendering/vulkan/vulkan_context.hpp"
#include "rendering/vulkan/vulkan_swapchain.hpp"
#include "utils/result.hpp"

namespace mole {

VulkanPresenter::VulkanPresenter(const VulkanContext& vk_context, const IWindow* window)
    : vk_context(&vk_context),
      window(window) {}

VulkanPresenter::~VulkanPresenter() = default;

Result<std::unique_ptr<VulkanPresenter>> VulkanPresenter::create(const VulkanContext& vk_context,
                                                                 const IWindow* window) {
  assert(window && "VulkanPresenter requires a window");
  std::unique_ptr<VulkanPresenter> presenter(new VulkanPresenter(vk_context, window));

  MOLE_TRY(presenter->create_surface());
  MOLE_TRY(presenter->create_sync_objects());

  // The swapchain may not be creatable yet (e.g. minimized window); acquire() retries in that case.
  MOLE_TRY(presenter->ensure_swapchain());

  return std::move(presenter);
}

Result<void> VulkanPresenter::create_surface() {
#ifdef MOLE_PLATFORM_WINDOWS
  vk::Win32SurfaceCreateInfoKHR create_info{
    .hinstance = GetModuleHandle(nullptr),
    .hwnd = static_cast<HWND>(window->get_native_handle())
  };

  vk::Instance raw_instance = *vk_context->get_instance();
  auto vkr_surface = raw_instance.createWin32SurfaceKHR(create_info);
  if (vkr_surface.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create Win32 surface");
  }
  surface = vk::raii::SurfaceKHR(vk_context->get_instance(), vkr_surface.value);
#else
  return mole::fail("VulkanPresenter surface creation is only implemented for Windows");
#endif

  auto vkr_support =
      vk_context->get_physical_device().getSurfaceSupportKHR(vk_context->get_graphics_queue_index(), *surface);
  if (vkr_support.result != vk::Result::eSuccess || !vkr_support.value) {
    return mole::fail("The graphics queue can't present to this window's surface");
  }

  return mole::ok();
}

Result<void> VulkanPresenter::create_sync_objects() {
  for (vk::raii::Semaphore& semaphore : image_available) {
    auto vkr_semaphore = vk_context->get_logical_device().createSemaphore(vk::SemaphoreCreateInfo());
    if (vkr_semaphore.result != vk::Result::eSuccess) return mole::fail("Failed to create image available semaphore");
    semaphore = std::move(vkr_semaphore.value);
  }
  return mole::ok();
}

Result<void> VulkanPresenter::create_render_finished_semaphores() {
  render_finished.clear();
  for (size_t i = 0; i < swapchain->get_image_count(); i++) {
    auto vkr_semaphore = vk_context->get_logical_device().createSemaphore(vk::SemaphoreCreateInfo());
    if (vkr_semaphore.result != vk::Result::eSuccess) return mole::fail("Failed to create render finished semaphore");
    render_finished.push_back(std::move(vkr_semaphore.value));
  }
  return mole::ok();
}

Result<bool> VulkanPresenter::ensure_swapchain() {
  if (window->is_minimized()) {
    return false;
  }

  auto vkr_caps = vk_context->get_physical_device().getSurfaceCapabilitiesKHR(*surface);
  if (vkr_caps.result != vk::Result::eSuccess) {
    return mole::fail("Failed to query surface capabilities (surface lost?)");
  }
  const vk::SurfaceCapabilitiesKHR& caps = vkr_caps.value;

  // The surface's currentExtent is authoritative; the window size is only the fallback when it's undefined.
  vk::Extent2D target_extent = caps.currentExtent;
  if (caps.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
    target_extent = vk::Extent2D{
      .width = std::clamp(window->get_framebuffer_width(), caps.minImageExtent.width, caps.maxImageExtent.width),
      .height = std::clamp(window->get_framebuffer_height(), caps.minImageExtent.height, caps.maxImageExtent.height)
    };
  }

  if (target_extent.width == 0 || target_extent.height == 0) {
    return false;
  }

  const bool extent_changed = !swapchain || swapchain->get_extent() != target_extent;
  if (!needs_recreate && !extent_changed) {
    return true;
  }

  if (!swapchain) {
    MOLE_TRY_ASSIGN(swapchain, VulkanSwapchain::create(*vk_context, surface, target_extent));
  } else {
    MOLE_TRY(swapchain->recreate(*vk_context, surface, target_extent));
  }

  if (render_finished.size() != swapchain->get_image_count()) {
    MOLE_TRY(create_render_finished_semaphores());
  }

  needs_recreate = false;
  return true;
}

Result<bool> VulkanPresenter::acquire(uint32_t frame_index) {
  if (acquired) {
    return true;
  }

  for (int attempt = 0; attempt < 2; ++attempt) {
    MOLE_TRY_ASSIGN(bool renderable, ensure_swapchain());
    if (!renderable) {
      return false;
    }

    SwapchainAcquireResult acquire_result = swapchain->acquire_next_image(image_available[frame_index]);
    switch (acquire_result.status) {
      case ESwapchainStatus::Suboptimal:
        // The image was acquired (and the semaphore will be signaled), so use it and recreate next time.
        needs_recreate = true;
        [[fallthrough]];
      case ESwapchainStatus::Ok:
        acquired = true;
        acquired_image_index = acquire_result.image_index;
        acquired_image_layout = vk::ImageLayout::eUndefined;
        return true;
      case ESwapchainStatus::OutOfDate:
        needs_recreate = true;
        continue;
      case ESwapchainStatus::SurfaceLost:
        return mole::fail("Surface lost while acquiring swapchain image");
      case ESwapchainStatus::DeviceLost:
        return mole::fail("Device lost while acquiring swapchain image");
      case ESwapchainStatus::Error:
        return mole::fail("Failed to acquire swapchain image");
    }
  }

  // Still out of date after one recreate-and-retry (live resize): skip this frame.
  return false;
}

void VulkanPresenter::release() {
  acquired = false;
}

void VulkanPresenter::mark_needs_recreate() {
  needs_recreate = true;
}

bool VulkanPresenter::is_acquired() const {
  return acquired;
}

uint32_t VulkanPresenter::get_acquired_image_index() const {
  return acquired_image_index;
}

vk::Image VulkanPresenter::get_acquired_image() const {
  if (!acquired) return nullptr;
  return swapchain->get_images()[acquired_image_index];
}

vk::ImageView VulkanPresenter::get_acquired_image_view() const {
  if (!acquired) return nullptr;
  return *swapchain->get_image_views()[acquired_image_index];
}

vk::ImageLayout& VulkanPresenter::get_acquired_image_layout() {
  return acquired_image_layout;
}

const vk::raii::Semaphore& VulkanPresenter::get_image_available(uint32_t frame_index) const {
  return image_available[frame_index];
}

const vk::raii::Semaphore& VulkanPresenter::get_render_finished() const {
  return render_finished[acquired_image_index];
}

vk::SwapchainKHR VulkanPresenter::get_swapchain() const {
  if (!swapchain) return nullptr;
  return *swapchain->get_swapchain();
}

vk::Extent2D VulkanPresenter::get_extent() const {
  if (!swapchain) return vk::Extent2D{};
  return swapchain->get_extent();
}

vk::Format VulkanPresenter::get_format() const {
  if (!swapchain) return vk::Format::eUndefined;
  return swapchain->get_surface_format().format;
}

uint32_t VulkanPresenter::get_image_count() const {
  if (!swapchain) return 0;
  return static_cast<uint32_t>(swapchain->get_image_count());
}

} // mole
