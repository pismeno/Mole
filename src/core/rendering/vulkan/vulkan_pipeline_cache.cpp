#include "rendering/vulkan/vulkan_pipeline_cache.hpp"

#include <cassert>
#include <memory>
#include <utility>

#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/combined_shader_key.hpp"
#include "rendering/vulkan/vulkan_context.hpp"
#include "rendering/vulkan/vulkan_material_handler.hpp"
#include "rendering/vulkan/vulkan_pipeline.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

VulkanPipelineCache::VulkanPipelineCache(
    VulkanContext& vk_context,
    VulkanMaterialHandler& material_handler,
    vk::raii::DescriptorSetLayout& descriptor_set_layout)
    : vk_context(vk_context),
      material_handler(material_handler),
      descriptor_set_layout(descriptor_set_layout) {}

VulkanPipelineCache::~VulkanPipelineCache() = default;

Result<VulkanPipeline*> VulkanPipelineCache::get(const AssetHandle<VertexShaderAsset>& vert,
                                                 const AssetHandle<FragmentShaderAsset>& frag,
                                                 vk::Format color_format) {
  PipelineKey key{
    .shader_key = CombinedShaderKey{
      vert.get_packed_id(),
      frag.get_packed_id()
    },
    .color_format = static_cast<uint32_t>(color_format)
  };
  PipelineShadersVersion current_version{vert.get_version(), frag.get_version()};
  auto it_pipelines = pipelines.find(key);
  if (it_pipelines != pipelines.end()) {
    auto it_versions = pipeline_shaders_versions.find(key);
    assert(it_versions != pipeline_shaders_versions.end() && "The version of the shaders should be containted along with the pipeline itself");

    PipelineShadersVersion& pipeline_version = it_versions->second;
    if (pipeline_version == current_version) {
      return it_pipelines->second.get();
    }
  }

  VulkanPipelineOptions create_options{
    .color_format = color_format,
    .vk_context = vk_context,
    .material_handler = material_handler,
    .descriptor_set_layout = descriptor_set_layout,
    .vert_shader = vert,
    .frag_shader = frag,
  };

  auto r_pipeline = VulkanPipeline::create(create_options);
  if (!r_pipeline) return mole::fail(r_pipeline.error());

  // If replacing an existing pipeline, ensure GPU has finished executing any in-flight command buffers referencing the old pipeline before it is destroyed.
  if (it_pipelines != pipelines.end()) {
    vk_context.get_logical_device().waitIdle();
  }

  pipelines[key] = std::move(r_pipeline).value();
  pipeline_shaders_versions[key] = current_version;

  return pipelines[key].get();
}

} // mole
