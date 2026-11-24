#include "rendering/vulkan/vulkan_render_target.hpp"

#include <memory>
#include <utility>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/renderer_types.hpp"
#include "rendering/vulkan/vulkan_context.hpp"
#include "rendering/vulkan/vulkan_memory_allocator.hpp"
#include "utils/result.hpp"

namespace mole {

vk::Format to_vk_format(ERenderTargetFormat format) {
  switch (format) {
    case ERenderTargetFormat::RGBA8_UNORM:
      return vk::Format::eR8G8B8A8Unorm;
    case ERenderTargetFormat::RGBA8_SRGB:
      return vk::Format::eR8G8B8A8Srgb;
    case ERenderTargetFormat::BGRA8_SRGB:
      return vk::Format::eB8G8R8A8Srgb;
    case ERenderTargetFormat::RGBA16_FLOAT:
      return vk::Format::eR16G16B16A16Sfloat;
  }
  return vk::Format::eUndefined;
}

VulkanRenderTarget::VulkanRenderTarget(const RenderTargetOptions& options)
    : options(options) {}

Result<std::unique_ptr<VulkanRenderTarget>> VulkanRenderTarget::create(const VulkanContext& vk_context,
                                                                       const VulkanMemoryAllocator& memory_allocator,
                                                                       const RenderTargetOptions& options) {
  if (options.width == 0 || options.height == 0) {
    return mole::fail("Render target size must be non-zero");
  }

  std::unique_ptr<VulkanRenderTarget> render_target(new VulkanRenderTarget(options));
  MOLE_TRY(render_target->init(vk_context, memory_allocator));
  return std::move(render_target);
}

Result<void> VulkanRenderTarget::init(const VulkanContext& vk_context, const VulkanMemoryAllocator& memory_allocator) {
  const vk::Format format = get_format();
  if (format == vk::Format::eUndefined) {
    return mole::fail("Unsupported render target format");
  }

  vk::ImageCreateInfo image_info{
    .imageType = vk::ImageType::e2D,
    .format = format,
    .extent = {
      options.width,
      options.height,
      1
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = vk::SampleCountFlagBits::e1,
    .tiling = vk::ImageTiling::eOptimal,
    .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
             vk::ImageUsageFlagBits::eTransferSrc,
    .sharingMode = vk::SharingMode::eExclusive,
    .initialLayout = vk::ImageLayout::eUndefined
  };

  vma::AllocationCreateInfo alloc_info{
    .flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
    .usage = vma::MemoryUsage::eAuto
  };

  auto vkr_image = memory_allocator.get_allocator().createImage(image_info, alloc_info);
  if (vkr_image.result != vk::Result::eSuccess) return mole::fail("Failed to create render target image");
  image = std::move(vkr_image.value);

  vk::ImageViewCreateInfo view_info{
    .image = *image,
    .viewType = vk::ImageViewType::e2D,
    .format = format,
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  };

  auto vkr_view = vk_context.get_logical_device().createImageView(view_info);
  if (vkr_view.result != vk::Result::eSuccess) return mole::fail("Failed to create render target image view");
  image_view = std::move(vkr_view.value);

  return mole::ok();
}

vk::Image VulkanRenderTarget::get_image() const {
  return *image;
}

vk::ImageView VulkanRenderTarget::get_image_view() const {
  return *image_view;
}

vk::Extent2D VulkanRenderTarget::get_extent() const {
  return vk::Extent2D{
    .width = options.width,
    .height = options.height
  };
}

vk::Format VulkanRenderTarget::get_format() const {
  return to_vk_format(options.format);
}

ERenderTargetFormat VulkanRenderTarget::get_options_format() const {
  return options.format;
}

} // mole
