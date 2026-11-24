#include "rendering/vulkan/vulkan_swapchain.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/vulkan_context.hpp"
#include "utils/result.hpp"

namespace mole {

Result<std::unique_ptr<VulkanSwapchain>> VulkanSwapchain::create(const VulkanContext& vk_context,
                                                                 const vk::raii::SurfaceKHR& surface,
                                                                 vk::Extent2D extent) {
  std::unique_ptr<VulkanSwapchain> swapchain(new VulkanSwapchain());

  MOLE_TRY(swapchain->create_swap_chain(vk_context, surface, extent));
  MOLE_TRY(swapchain->create_image_views(vk_context));

  return std::move(swapchain);
}

ESwapchainStatus to_swapchain_status(vk::Result result) {
  switch (result) {
    case vk::Result::eSuccess:
      return ESwapchainStatus::Ok;
    case vk::Result::eSuboptimalKHR:
      return ESwapchainStatus::Suboptimal;
    case vk::Result::eErrorOutOfDateKHR:
      return ESwapchainStatus::OutOfDate;
    case vk::Result::eErrorSurfaceLostKHR:
      return ESwapchainStatus::SurfaceLost;
    case vk::Result::eErrorDeviceLost:
      return ESwapchainStatus::DeviceLost;
    default:
      return ESwapchainStatus::Error;
  }
}

vk::SurfaceFormatKHR choose_swap_surface_format(std::vector<vk::SurfaceFormatKHR> const &availableFormats) {
  const auto formatIt = std::ranges::find_if(
      availableFormats,
      [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR choose_swap_present_mode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
  assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {return presentMode == vk::PresentModeKHR::eFifo;}));
  return std::ranges::any_of(availablePresentModes,
                             [](const vk::PresentModeKHR value) {return vk::PresentModeKHR::eMailbox == value;}) ?
         vk::PresentModeKHR::eMailbox :
         vk::PresentModeKHR::eFifo;
}

uint32_t choose_swap_min_image_count(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
  {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

Result<void> VulkanSwapchain::create_swap_chain(const VulkanContext& vk_context, const vk::raii::SurfaceKHR& surface,
                                               vk::Extent2D extent) {
  const vk::raii::PhysicalDevice& physical_device = vk_context.get_physical_device();

  auto caps_res = physical_device.getSurfaceCapabilitiesKHR(*surface);
  if (caps_res.result != vk::Result::eSuccess) return mole::fail("Failed to get surface capabilities");
  vk::SurfaceCapabilitiesKHR surface_capabilities = caps_res.value;

  auto formats_res = physical_device.getSurfaceFormatsKHR(*surface);
  if (formats_res.result != vk::Result::eSuccess) return mole::fail("Failed to get surface formats");
  std::vector<vk::SurfaceFormatKHR> available_formats = formats_res.value;

  auto modes_res = physical_device.getSurfacePresentModesKHR(*surface);
  if (modes_res.result != vk::Result::eSuccess) return mole::fail("Failed to get present modes");
  std::vector<vk::PresentModeKHR> availablePresentModes = modes_res.value;

  swap_chain_extent = extent;
  if (swap_chain_extent.width == 0 || swap_chain_extent.height == 0) {
    return mole::fail("Cannot create swapchain with zero extent");
  }
  uint32_t minImageCount = choose_swap_min_image_count(surface_capabilities);
  swap_chain_surface_format = choose_swap_surface_format(available_formats);

  vk::SwapchainCreateInfoKHR swap_chain_create_info{
      .surface          = *surface,
      .minImageCount    = minImageCount,
      .imageFormat      = swap_chain_surface_format.format,
      .imageColorSpace  = swap_chain_surface_format.colorSpace,
      .imageExtent      = swap_chain_extent,
      .imageArrayLayers = 1,
      .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform     = surface_capabilities.currentTransform,
      .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode      = choose_swap_present_mode(availablePresentModes),
      .clipped          = true
  };

  const vk::raii::Device& logical_device = vk_context.get_logical_device();
  vk::Device raw_dev = *logical_device;
  auto swap_res = raw_dev.createSwapchainKHR(swap_chain_create_info);
  if (swap_res.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create swapchain");
  }
  swap_chain = vk::raii::SwapchainKHR(logical_device, swap_res.value);

  auto images_res = swap_chain.getImages();
  if (images_res.result != vk::Result::eSuccess) {
    return mole::fail("Failed to retrieve swapchain images");
  }
  swap_chain_images = images_res.value;

  return mole::ok();
}

Result<void> VulkanSwapchain::create_image_views(const VulkanContext& vk_context) {
  assert(swap_chain_image_views.empty());

  vk::ImageViewCreateInfo image_view_create_info{
      .viewType         = vk::ImageViewType::e2D,
      .format           = swap_chain_surface_format.format,
      .components       = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity},
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1},
  };

  const vk::raii::Device& logical_device = vk_context.get_logical_device();
  vk::Device raw_device = *logical_device;
  for (auto &image : swap_chain_images)
  {
    image_view_create_info.image = image;

    auto view_res = raw_device.createImageView(image_view_create_info);
    if (view_res.result != vk::Result::eSuccess) {
      return mole::fail("Failed to create swapchain image view");
    }

    swap_chain_image_views.emplace_back(logical_device, view_res.value);
  }

  return mole::ok();
}

Result<void> VulkanSwapchain::recreate(const VulkanContext& vk_context, const vk::raii::SurfaceKHR& surface,
                                      vk::Extent2D extent) {
  if (vk_context.get_logical_device().waitIdle() != vk::Result::eSuccess) {
    return mole::fail("Failed to wait for device idle before swapchain recreation");
  }

  swap_chain_image_views.clear();
  swap_chain_images.clear();
  swap_chain = nullptr;

  // TODO later it should be done directly here to pass current swap chain into create swap chain struct as an old to not lose render commands
  MOLE_TRY(create_swap_chain(vk_context, surface, extent));
  MOLE_TRY(create_image_views(vk_context));

  return mole::ok();
}

SwapchainAcquireResult VulkanSwapchain::acquire_next_image(const vk::raii::Semaphore& image_available_semaphore) {
  auto acquire_res = swap_chain.acquireNextImage(UINT64_MAX, *image_available_semaphore, nullptr);
  SwapchainAcquireResult result{
    .status = to_swapchain_status(acquire_res.result),
    .image_index = acquire_res.value
  };
  return result;
}

const vk::raii::SwapchainKHR& VulkanSwapchain::get_swapchain() const {
  return swap_chain;
}

} // mole
