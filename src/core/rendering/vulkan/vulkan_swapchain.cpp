#include "vulkan_swapchain.hpp"

#include <memory>

#include <vulkan/vulkan_raii.hpp>

namespace mole {

Result<std::unique_ptr<VulkanSwapchain>> VulkanSwapchain::create(VulkanContext& vk_context, uint32_t window_width, uint32_t window_height) {
    std::unique_ptr<VulkanSwapchain> swapchain(new VulkanSwapchain());

    auto r_create_swap_chain = swapchain->create_swap_chain(vk_context, window_width, window_height);
    if (!r_create_swap_chain) return mole::fail(r_create_swap_chain.error());

    auto r_create_image_views = swapchain->create_image_views(vk_context);
    if (!r_create_image_views) return mole::fail(r_create_image_views.error());

    return std::move(swapchain);
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

vk::Extent2D choose_swap_extent(vk::SurfaceCapabilitiesKHR const &capabilities, uint32_t window_width, uint32_t window_height)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }

  return {
      std::clamp<uint32_t>(window_width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
      std::clamp<uint32_t>(window_height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
  };
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

Result<void> VulkanSwapchain::create_swap_chain(VulkanContext& vk_context, uint32_t window_width, uint32_t window_height) {
  const vk::raii::PhysicalDevice& physical_device = vk_context.get_physical_device();
  const vk::raii::SurfaceKHR& surface = vk_context.get_surface();

  auto caps_res = physical_device.getSurfaceCapabilitiesKHR(*surface);
  if (caps_res.result != vk::Result::eSuccess) return mole::fail("Failed to get surface capabilities");
  vk::SurfaceCapabilitiesKHR surface_capabilities = caps_res.value;

  auto formats_res = physical_device.getSurfaceFormatsKHR(*surface);
  if (formats_res.result != vk::Result::eSuccess) return mole::fail("Failed to get surface formats");
  std::vector<vk::SurfaceFormatKHR> available_formats = formats_res.value;

  auto modes_res = physical_device.getSurfacePresentModesKHR(*surface);
  if (modes_res.result != vk::Result::eSuccess) return mole::fail("Failed to get present modes");
  std::vector<vk::PresentModeKHR> availablePresentModes = modes_res.value;

  swap_chain_extent = choose_swap_extent(surface_capabilities, window_width, window_height);
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

Result<void> VulkanSwapchain::create_image_views(VulkanContext& vk_context) {
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

Result<void> VulkanSwapchain::recreate(VulkanContext& vk_context, uint32_t window_width, uint32_t window_height) {
  if (window_width == 0 || window_height == 0) {
    return mole::fail("Cannot recreate swapchain with zero dimensions");
  }

  const vk::raii::PhysicalDevice& physical_device = vk_context.get_physical_device();
  const vk::raii::SurfaceKHR& surface = vk_context.get_surface();

  auto caps_res = physical_device.getSurfaceCapabilitiesKHR(*surface);
  if (caps_res.result != vk::Result::eSuccess) return mole::fail("Failed to get surface capabilities");
  if (caps_res.value.currentExtent.width == 0 && caps_res.value.currentExtent.height == 0) {
    return mole::fail("Cannot recreate swapchain while window is minimized");
  }

  vk_context.get_logical_device().waitIdle();

  swap_chain_image_views.clear();
  swap_chain = nullptr;

  auto r_create_swap_chain = create_swap_chain(vk_context, window_width, window_height); // TODO later it should be done directly here to pass current swap chain into create swap chain struct as an old to not lose render commands
  if (!r_create_swap_chain) return r_create_swap_chain;

  auto r_create_image_views = create_image_views(vk_context);
  if (!r_create_image_views) return r_create_image_views;

  return mole::ok();
}

Result<uint32_t> VulkanSwapchain::acquire_next_image(const vk::raii::Semaphore& present_complete_semaphore) {
  // Use UINT64_MAX to disable the timeout
  auto acquire_res = swap_chain.acquireNextImage(UINT64_MAX, *present_complete_semaphore, nullptr);

  // Check for window resize conditions
  if (acquire_res.result == vk::Result::eErrorOutOfDateKHR || acquire_res.result == vk::Result::eSuboptimalKHR) {
    return mole::fail("Swapchain out of date or suboptimal");
  } else if (acquire_res.result == vk::Result::eErrorDeviceLost) {
    return mole::fail("Device lost");
  } else if (acquire_res.result != vk::Result::eSuccess) {
    return mole::fail("Failed to acquire next swapchain image");
  }

  return acquire_res.value;
}

Result<void> VulkanSwapchain::present(const vk::raii::Queue& present_queue, const vk::raii::Semaphore& render_finished_semaphore, uint32_t image_index) {
  vk::PresentInfoKHR present_info{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores    = &*render_finished_semaphore,
      .swapchainCount     = 1,
      .pSwapchains        = &*swap_chain,
      .pImageIndices      = &image_index
  };

  // presentKHR returns a vk::Result directly, not a ResultValue
  vk::Result present_res = present_queue.presentKHR(present_info);

  // Check for window resize conditions
  if (present_res == vk::Result::eErrorOutOfDateKHR || present_res == vk::Result::eSuboptimalKHR) {
    return mole::fail("Swapchain out of date or suboptimal");
  } else if (present_res == vk::Result::eErrorDeviceLost) {
    return mole::fail("Device lost");
  } else if (present_res != vk::Result::eSuccess) {
    return mole::fail("Failed to present swapchain image");
  }

  return mole::ok();
}

} // mole
