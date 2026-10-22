#pragma once

#include <memory>

#include <vulkan/vulkan_raii.hpp>

#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

class VulkanContext;
class VulkanMaterialHandler;
class VulkanSwapchain;

struct VulkanPipelineOptions {
  VulkanSwapchain& swapchain;
  VulkanContext& vk_context;
  VulkanMaterialHandler& material_handler;
  vk::raii::DescriptorSetLayout& descriptor_set_layout;
  const AssetHandle<VertexShaderAsset>& vert_shader;
  const AssetHandle<FragmentShaderAsset>& frag_shader;
};

class VulkanPipeline {
 public:
  ~VulkanPipeline() = default;

  inline const vk::raii::PipelineLayout& get_layout() const { return pipeline_layout; }
  inline const vk::raii::Pipeline& get_pipeline() const { return graphics_pipeline; }

  void bind(const vk::raii::CommandBuffer& command_buffer) const;

  static Result<std::unique_ptr<VulkanPipeline>> create(const VulkanPipelineOptions& options);
 private:
  explicit VulkanPipeline() = default;

  Result<void> create_graphics_pipeline(const VulkanPipelineOptions& options);

  vk::raii::PipelineLayout pipeline_layout = nullptr;
  vk::raii::Pipeline graphics_pipeline = nullptr;
};

} // mole
