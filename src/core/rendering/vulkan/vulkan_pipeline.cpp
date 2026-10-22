#include "rendering/vulkan/vulkan_pipeline.hpp"

#include <array>
#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/vulkan_context.hpp"
#include "rendering/vulkan/vulkan_material_handler.hpp"
#include "rendering/vulkan/vulkan_swapchain.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

Result<std::unique_ptr<VulkanPipeline>> VulkanPipeline::create(const VulkanPipelineOptions& options) {
  std::unique_ptr<VulkanPipeline> pipeline(new VulkanPipeline());

  auto r_create_graphics_pipeline = pipeline->create_graphics_pipeline(options);
  if (!r_create_graphics_pipeline) return mole::fail(r_create_graphics_pipeline.error());

  return std::move(pipeline);
}

template <std::derived_from<ShaderAsset> A>
Result<std::unique_ptr<vk::raii::ShaderModule>> load_shader_module(const AssetHandle<A>& shader_handle, VulkanContext& vk_context) {
  vk::ShaderModuleCreateInfo create_info{
      .codeSize = shader_handle->data.size(),      // Size in bytes
      .pCode    = shader_handle->as_32bit_words()  // Pointer to aligned uint32_t data
  };

  auto vkr_create_shader_module = vk_context.get_logical_device().createShaderModule(create_info);
  if (vkr_create_shader_module.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create Vulkan shader module for");
  }

  return std::make_unique<vk::raii::ShaderModule>(std::move(vkr_create_shader_module.value));
}

Result<void> VulkanPipeline::create_graphics_pipeline(const VulkanPipelineOptions& options) {
  // loading the shaders from files
  auto r_load_vert_shader_module = load_shader_module(options.vert_shader, options.vk_context);
  if (!r_load_vert_shader_module) return mole::fail(r_load_vert_shader_module.error());

  auto r_load_frag_shader_module = load_shader_module(options.frag_shader, options.vk_context);
  if (!r_load_frag_shader_module) return mole::fail(r_load_frag_shader_module.error());

  std::unique_ptr<vk::raii::ShaderModule> vert_shader_module = std::move(r_load_vert_shader_module).value();
  std::unique_ptr<vk::raii::ShaderModule> frag_shader_module = std::move(r_load_frag_shader_module).value();

  // vertex shader stage create info
  vk::PipelineShaderStageCreateInfo vert_shader_stage_info{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = **vert_shader_module,
      .pName = "main"
  };

  // fragment shader stage create info
  vk::PipelineShaderStageCreateInfo frag_shader_stage_info{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = **frag_shader_module,
      .pName = "main"
  };

  vk::PipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

  std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

  // pipeline layout create info
  vk::PipelineDynamicStateCreateInfo dynamic_state{
      .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data()
  };

  // vertex input create info
  vk::PipelineVertexInputStateCreateInfo vertex_input_info{
      .vertexBindingDescriptionCount   = 0,
      .pVertexBindingDescriptions      = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions    = nullptr
  };

  // input assembly create info
  vk::PipelineInputAssemblyStateCreateInfo input_assembly{.topology = vk::PrimitiveTopology::eTriangleList};

  const vk::Extent2D& swap_chain_extent = options.swapchain.get_extent();
  // viewport and scissor
  vk::Viewport viewport{0.0f, static_cast<float>(swap_chain_extent.height), static_cast<float>(swap_chain_extent.width), -static_cast<float>(swap_chain_extent.height), 0.0f, 1.0f};
  vk::Rect2D scissor{vk::Offset2D{ 0, 0 }, swap_chain_extent};

  // viewport create info
  vk::PipelineViewportStateCreateInfo viewport_state{
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor
  };

  // rasterizer create info
  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable        = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode             = vk::PolygonMode::eFill,
      .cullMode                = vk::CullModeFlagBits::eNone,
      .frontFace               = vk::FrontFace::eClockwise,
      .depthBiasEnable         = vk::False,
      .lineWidth               = 1.0f
  };

  // multisampling create info
  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False
  };

  // disabling color blending for our only framebuffer
  vk::PipelineColorBlendAttachmentState color_blend_attachment{
      .blendEnable    = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp        = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp        = vk::BlendOp::eAdd,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
  };

  vk::PipelineColorBlendStateCreateInfo color_blending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment
  };

  std::array<vk::DescriptorSetLayout, 2> set_layouts = {
      options.descriptor_set_layout,                                        // Set 0: UBO
      options.material_handler.get_descriptor_set_layout()                  // Set 1: Bindless Textures
  };

  // pipeline layout create info
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
      .pSetLayouts    = set_layouts.data(),
  };

  const vk::raii::Device& logical_device = options.vk_context.get_logical_device();

  // creating the pipeline layout
  auto vkr_create_pipeline_layout = logical_device.createPipelineLayout(pipeline_layout_info);
  if (vkr_create_pipeline_layout.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create pipeline layout");
  }

  pipeline_layout = std::move(vkr_create_pipeline_layout.value);

  vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info_chain = {
      {
          .stageCount          = 2,
          .pStages             = shader_stages,
          .pVertexInputState   = &vertex_input_info,
          .pInputAssemblyState = &input_assembly,
          .pViewportState      = &viewport_state,
          .pRasterizationState = &rasterizer,
          .pMultisampleState   = &multisampling,
          .pColorBlendState    = &color_blending,
          .pDynamicState       = &dynamic_state,
          .layout              = pipeline_layout,
          .renderPass          = nullptr
      },
      {
          .colorAttachmentCount    = 1,
          .pColorAttachmentFormats = &options.swapchain.get_surface_format().format
      }
  };


  auto vkr_create_graphics_pipeline = logical_device.createGraphicsPipeline(nullptr, pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());
  if (vkr_create_graphics_pipeline.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create graphics pipeline");
  }
  graphics_pipeline = std::move(vkr_create_graphics_pipeline.value);

  return mole::ok();
}

void VulkanPipeline::bind(const vk::raii::CommandBuffer& command_buffer) const {
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphics_pipeline);
}

} // mole