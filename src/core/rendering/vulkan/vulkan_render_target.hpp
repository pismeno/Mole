#pragma once

#include <cstdint>
#include <memory>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/renderer_types.hpp"
#include "utils/result.hpp"

namespace mole {

class VulkanContext;
class VulkanMemoryAllocator;

/** @brief Maps the API-agnostic render target format to its Vulkan format. */
[[nodiscard]] vk::Format to_vk_format(ERenderTargetFormat format);

/** @brief Private helper of VulkanRenderer: an offscreen color image that can be rendered into and sampled. */
class VulkanRenderTarget {
public:
  ~VulkanRenderTarget() = default;

  [[nodiscard]] vk::Image get_image() const;
  [[nodiscard]] vk::ImageView get_image_view() const;
  [[nodiscard]] vk::Extent2D get_extent() const;
  [[nodiscard]] vk::Format get_format() const;
  [[nodiscard]] ERenderTargetFormat get_options_format() const;

  /** @brief Layout the image is in once all recorded commands have executed. */
  vk::ImageLayout current_layout = vk::ImageLayout::eUndefined;

  static Result<std::unique_ptr<VulkanRenderTarget>> create(const VulkanContext& vk_context,
                                                            const VulkanMemoryAllocator& memory_allocator,
                                                            const RenderTargetOptions& options);

private:
  explicit VulkanRenderTarget(const RenderTargetOptions& options);

  Result<void> init(const VulkanContext& vk_context, const VulkanMemoryAllocator& memory_allocator);

  RenderTargetOptions options;
  vma::raii::Image image = nullptr;
  vk::raii::ImageView image_view = nullptr;
};

} // mole
