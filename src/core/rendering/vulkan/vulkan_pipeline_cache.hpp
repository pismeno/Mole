#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/combined_shader_key.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

class VulkanContext;
class VulkanMaterialHandler;
class VulkanPipeline;
class VulkanSwapchain;

class VulkanPipelineCache {
 public:
  explicit VulkanPipelineCache(
      VulkanSwapchain& swapchain,
      VulkanContext& vk_context,
      VulkanMaterialHandler& material_handler,
      vk::raii::DescriptorSetLayout& descriptor_set_layout);
  ~VulkanPipelineCache();

  Result<VulkanPipeline*> get(const AssetHandle<VertexShaderAsset>& vert, const AssetHandle<FragmentShaderAsset>& frag);
 private:
  std::unordered_map<CombinedShaderKey, std::unique_ptr<VulkanPipeline>, CombinedShaderKeyHash> pipelines;
  std::unordered_map<CombinedShaderKey, PipelineShadersVersion, CombinedShaderKeyHash> pipeline_shaders_versions;
  VulkanSwapchain& swapchain;
  VulkanContext& vk_context;
  VulkanMaterialHandler& material_handler;
  vk::raii::DescriptorSetLayout& descriptor_set_layout;
};

} // mole
