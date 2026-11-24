#include "rendering/vulkan/vulkan_renderer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/rendering_context.hpp"
#include "rendering/vulkan/combined_shader_key.hpp"
#include "rendering/vulkan/quad.hpp"
#include "rendering/vulkan/render_batch.hpp"
#include "rendering/vulkan/vulkan_context.hpp"
#include "rendering/vulkan/vulkan_material_handler.hpp"
#include "rendering/vulkan/vulkan_memory_allocator.hpp"
#include "rendering/vulkan/vulkan_pipeline.hpp"
#include "rendering/vulkan/vulkan_pipeline_cache.hpp"
#include "rendering/vulkan/vulkan_presenter.hpp"
#include "rendering/vulkan/vulkan_render_target.hpp"
#include "rendering/vulkan/vulkan_swapchain.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/resources_context.hpp"
#include "utils/counter.hpp"
#include "utils/result.hpp"

namespace mole {

namespace {

static_assert(sizeof(void*) == 8, "Native handles are passed as void*, which requires 64-bit pointers");

template <typename T>
void* to_native_handle_ptr(T handle) {
  if constexpr (std::is_pointer_v<T>) {
    return const_cast<void*>(static_cast<const void*>(handle));
  } else {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
  }
}

struct LayoutAccess {
  vk::PipelineStageFlags2 stage;
  vk::AccessFlags2 access;
};

/// Source stage/access for a barrier out of `layout`, as left by this renderer.
LayoutAccess get_src_access(vk::ImageLayout layout) {
  switch (layout) {
    case vk::ImageLayout::eShaderReadOnlyOptimal:
      return LayoutAccess{
        .stage = vk::PipelineStageFlagBits2::eFragmentShader,
        .access = vk::AccessFlagBits2::eShaderSampledRead
      };
    case vk::ImageLayout::eColorAttachmentOptimal:
      return LayoutAccess{
        .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .access = vk::AccessFlagBits2::eColorAttachmentWrite
      };
    default:
      // eUndefined: chain with the acquire semaphore wait, which happens at color attachment output.
      return LayoutAccess{
        .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .access = {}
      };
  }
}

} // namespace

VulkanRenderer::VulkanRenderer(const RenderingContext& context) : 
  asset_manager(context.resources_context->asset_manager),
  material_pool(context.resources_context->material_pool) {}

VulkanRenderer::~VulkanRenderer() {
  // Waits return in finite time even on a lost device, so this is always safe.
  if (vk_context) {
    (void)vk_context->get_logical_device().waitIdle();
  }
}

Result<std::unique_ptr<VulkanRenderer>> VulkanRenderer::create(RenderingContext& context) {
  std::unique_ptr<VulkanRenderer> renderer(new VulkanRenderer(context));
  MOLE_TRY(renderer->init(context));
  return std::move(renderer);
}

Result<void> VulkanRenderer::init(RenderingContext& context) {
  frames_rendered = std::make_unique<Counter>();

  MOLE_TRY_ASSIGN(vk_context, VulkanContext::create());
  std::cout << "Vulkan Context created successfully" << std::endl;

  MOLE_TRY_ASSIGN(memory_allocator, VulkanMemoryAllocator::create(*vk_context));
  std::cout << "VMA Allocator created successfully" << std::endl;

  MOLE_TRY(create_command_pool());

  MOLE_TRY_ASSIGN(material_handler, VulkanMaterialHandler::create(*vk_context, *memory_allocator, *frames_rendered, context));
  std::cout << "Material Handler created successfully" << std::endl;

  MOLE_TRY(create_descriptor_set_layout());

  pipeline_cache = std::make_unique<VulkanPipelineCache>(*vk_context, *material_handler, descriptor_set_layout);

  MOLE_TRY(create_frame_data());

  std::cout << "Vulkan Renderer initialized successfully" << std::endl;
  return mole::ok();
}

Result<void> VulkanRenderer::create_command_pool() {
  vk::CommandPoolCreateInfo pool_info{
    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = vk_context->get_graphics_queue_index()
  };

  auto vkr_create_command_pool = vk_context->get_logical_device().createCommandPool(pool_info);
  if (vkr_create_command_pool.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create command pool");
  }

  command_pool = std::move(vkr_create_command_pool.value);
  return mole::ok();
}

Result<void> VulkanRenderer::create_descriptor_set_layout() {
  vk::DescriptorSetLayoutBinding ubo_binding{
    .binding = 0,
    .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
    .descriptorCount = 1,
    .stageFlags = vk::ShaderStageFlagBits::eVertex,
    .pImmutableSamplers = nullptr
  };

  vk::DescriptorSetLayoutBinding ssbo_binding{
    .binding = 1,
    .descriptorType = vk::DescriptorType::eStorageBuffer,
    .descriptorCount = 1,
    .stageFlags = vk::ShaderStageFlagBits::eVertex
  };

  std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
    ubo_binding,
    ssbo_binding
  };

  vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data()
  };

  auto vkr_create_desc_set_layout =
      vk_context->get_logical_device().createDescriptorSetLayout(descriptor_set_layout_info);
  if (vkr_create_desc_set_layout.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create descriptor set layout");
  }

  descriptor_set_layout = std::move(vkr_create_desc_set_layout.value);
  return mole::ok();
}

Result<void> VulkanRenderer::create_frame_data() {
  frames.resize(MAX_FRAMES_IN_FLIGHT);

  // One UBO slot per pass, aligned for dynamic offsets
  const size_t alignment = vk_context->get_physical_device().getProperties().limits.minUniformBufferOffsetAlignment;
  ubo_stride = sizeof(ViewData);
  if (alignment > 0) {
    ubo_stride = (ubo_stride + alignment - 1) & ~(alignment - 1);
  }

  std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
    vk::DescriptorPoolSize{
      .type = vk::DescriptorType::eUniformBufferDynamic,
      .descriptorCount = MAX_FRAMES_IN_FLIGHT
    },
    vk::DescriptorPoolSize{
      .type = vk::DescriptorType::eStorageBuffer,
      .descriptorCount = MAX_FRAMES_IN_FLIGHT
    }
  };

  vk::DescriptorPoolCreateInfo pool_info{
    .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    .maxSets = MAX_FRAMES_IN_FLIGHT,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data()
  };

  auto vkr_ubo_pool = vk_context->get_logical_device().createDescriptorPool(pool_info);
  if (vkr_ubo_pool.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create UBO descriptor pool");
  }
  ubo_descriptor_pool = std::move(vkr_ubo_pool.value);

  vk::CommandBufferAllocateInfo alloc_info{
    .commandPool = *command_pool,
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT
  };

  auto vkr_cmd_buffers = vk_context->get_logical_device().allocateCommandBuffers(alloc_info);
  if (vkr_cmd_buffers.result != vk::Result::eSuccess) {
    return mole::fail("Failed to allocate command buffers");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    FrameData& frame = frames[i];

    auto vkr_fence = vk_context->get_logical_device().createFence(vk::FenceCreateInfo{
      .flags = vk::FenceCreateFlagBits::eSignaled
    });
    if (vkr_fence.result != vk::Result::eSuccess) return mole::fail("Failed to create fence");
    frame.in_flight_fence = std::move(vkr_fence.value);

    frame.command_buffer = std::move(vkr_cmd_buffers.value[i]);

    MOLE_TRY_ASSIGN(frame.uniform_buffer, memory_allocator->create_buffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                                                          ubo_stride * MAX_PASSES_PER_FRAME));
    frame.uniform_buffer_mapped = static_cast<std::byte*>(frame.uniform_buffer.getAllocation().getInfo().pMappedData);

    MOLE_TRY_ASSIGN(frame.quad_buffer,
                    memory_allocator->create_buffer(vk::BufferUsageFlagBits::eStorageBuffer, sizeof(Quad) * MAX_QUADS));
    frame.quad_buffer_mapped = static_cast<Quad*>(frame.quad_buffer.getAllocation().getInfo().pMappedData);

    vk::DescriptorSetLayout raw_ubo_layout = *descriptor_set_layout;
    vk::DescriptorSetAllocateInfo desc_alloc_info{
      .descriptorPool = *ubo_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &raw_ubo_layout
    };

    auto vkr_desc_set = vk_context->get_logical_device().allocateDescriptorSets(desc_alloc_info);
    if (vkr_desc_set.result != vk::Result::eSuccess) {
      return mole::fail("Failed to allocate frame UBO descriptor set");
    }
    frame.descriptor_set = std::move(vkr_desc_set.value[0]);

    vk::DescriptorBufferInfo ubo_info{
      .buffer = *frame.uniform_buffer,
      .offset = 0,
      .range = sizeof(ViewData)
    };

    vk::DescriptorBufferInfo ssbo_info{
      .buffer = *frame.quad_buffer,
      .offset = 0,
      .range = sizeof(Quad) * MAX_QUADS
    };

    std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {
      vk::WriteDescriptorSet{
        .dstSet = *frame.descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
        .pBufferInfo = &ubo_info
      },
      vk::WriteDescriptorSet{
        .dstSet = *frame.descriptor_set,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = &ssbo_info
      }
    };

    vk_context->get_logical_device().updateDescriptorSets(descriptor_writes, nullptr);
  }

  return mole::ok();
}

Result<void> VulkanRenderer::begin_frame() {
  if (in_frame) return mole::fail("begin_frame called while a frame is already in progress");

  FrameData& frame = frames[current_frame];
  const vk::raii::Device& device = vk_context->get_logical_device();

  // 1. Wait for the GPU to finish with this frame's resources
  if (device.waitForFences(*frame.in_flight_fence, vk::True, UINT64_MAX) != vk::Result::eSuccess) {
    return mole::fail("Failed to wait for the in-flight fence (device lost?)");
  }

  // 2. Release render targets no in-flight frame can reference anymore
  std::erase_if(retired_render_targets, [this](const RetiredRenderTarget& retired) {
    return retired.retire_after_frame <= frames_rendered->count;
  });

  // 3. Reset only once we're sure this frame will be submitted in end_frame
  if (device.resetFences(*frame.in_flight_fence) != vk::Result::eSuccess) {
    return mole::fail("Failed to reset the in-flight fence");
  }

  if (frame.command_buffer.begin({}) != vk::Result::eSuccess) {
    return mole::fail("Failed to begin the frame command buffer");
  }

  pass_cursor = 0;
  quad_cursor = 0;
  acquired_presenters.clear();
  in_frame = true;
  return mole::ok();
}

Result<bool> VulkanRenderer::begin_pass(RenderTargetID target, const PassOptions& options) {
  if (!in_frame) return mole::fail("begin_pass called outside of a frame");
  if (active_pass) return mole::fail("begin_pass called while another pass is open");

  RenderTargetEntry* entry = render_targets.get(target);
  if (!entry) return mole::fail("begin_pass called with an invalid or stale RenderTargetID");

  ActivePass pass{
    .target = target,
    .view_data = options.view_data
  };
  vk::ImageView image_view;
  vk::Extent2D extent;

  // 1. Resolve the target
  if (auto* offscreen = std::get_if<std::unique_ptr<VulkanRenderTarget>>(entry)) {
    VulkanRenderTarget& render_target = **offscreen;
    pass.image = render_target.get_image();
    pass.format = render_target.get_format();
    pass.layout = &render_target.current_layout;
    pass.is_offscreen = true;
    image_view = render_target.get_image_view();
    extent = render_target.get_extent();
  } else {
    const PresenterID presenter_id = std::get<PresenterID>(*entry);
    PresenterEntry* presenter_entry = presenters.get(presenter_id);
    assert(presenter_entry && "A presenter RT must always reference a live presenter");
    VulkanPresenter& presenter = *presenter_entry->presenter;

    const bool was_acquired = presenter.is_acquired();
    MOLE_TRY_ASSIGN(bool renderable, presenter.acquire(current_frame));
    if (!renderable) {
      return false;
    }
    if (!was_acquired) {
      acquired_presenters.push_back(presenter_id);
    }

    pass.image = presenter.get_acquired_image();
    pass.format = presenter.get_format();
    pass.layout = &presenter.get_acquired_image_layout();
    pass.is_offscreen = false;
    image_view = presenter.get_acquired_image_view();
    extent = presenter.get_extent();
  }

  if (extent.width == 0 || extent.height == 0) {
    return false;
  }

  FrameData& frame = frames[current_frame];

  // 2. Transition to color attachment (contents are discarded when clearing)
  const vk::ImageLayout old_layout = options.clear_color ? vk::ImageLayout::eUndefined : *pass.layout;
  const LayoutAccess src = get_src_access(*pass.layout);
  transition_image_layout(frame.command_buffer, pass.image, old_layout, vk::ImageLayout::eColorAttachmentOptimal,
                          src.access,
                          vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
                          src.stage, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  *pass.layout = vk::ImageLayout::eColorAttachmentOptimal;

  // 3. Begin rendering
  const glm::vec4 clear = options.clear_color.value_or(glm::vec4(0.0f));
  vk::RenderingAttachmentInfo attachment_info{
    .imageView = image_view,
    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
    .loadOp = options.clear_color ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = vk::ClearColorValue(clear.r, clear.g, clear.b, clear.a)
  };

  vk::RenderingInfo rendering_info{
    .renderArea = {
      .offset = {
        0,
        0
      },
      .extent = extent
    },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &attachment_info
  };

  frame.command_buffer.beginRendering(rendering_info);

  // 4. Dynamic viewport/scissor from the target extent (negative height flips Y)
  frame.command_buffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(extent.height),
                                                   static_cast<float>(extent.width),
                                                   -static_cast<float>(extent.height), 0.0f, 1.0f));
  frame.command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

  // 5. Remember the pass
  active_pass = pass;
  return true;
}

void VulkanRenderer::submit(const RenderQuadCommand& render_command) { pending_commands.push_back(render_command); }

void VulkanRenderer::submit(const std::vector<RenderQuadCommand>& render_commands) {
  pending_commands.insert(pending_commands.end(), render_commands.begin(), render_commands.end());
}

Result<void> VulkanRenderer::end_pass() {
  if (!active_pass) return mole::fail("end_pass called without an open pass");

  FrameData& frame = frames[current_frame];
  const ActivePass pass = *active_pass;
  active_pass.reset();

  if (!pending_commands.empty()) {
    // Per-material problems are logged by the handler; only Vulkan failures come back here.
    MOLE_TRY(material_handler->prepare_materials(get_needed_pass_materials()));
    build_batches();
    record_draws(frame, pass);
  }

  frame.command_buffer.endRendering();

  if (pass.is_offscreen) {
    transition_image_layout(frame.command_buffer, pass.image, vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eColorAttachmentWrite,
                            vk::AccessFlagBits2::eShaderSampledRead,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eFragmentShader);
    *pass.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
  }

  return mole::ok();
}

void VulkanRenderer::record_draws(FrameData& frame, const ActivePass& pass) {
  if (quads.empty()) return;

  if (pass_cursor >= MAX_PASSES_PER_FRAME) {
    std::cerr << "VulkanRenderer: more than " << MAX_PASSES_PER_FRAME << " passes with draws this frame, dropping pass"
              << std::endl;
    return;
  }
  if (quad_cursor + quads.size() > MAX_QUADS) {
    std::cerr << "VulkanRenderer: frame exceeds " << MAX_QUADS << " quads, dropping pass" << std::endl;
    return;
  }

  // Each pass gets its own UBO slot and quad range so later passes don't overwrite earlier ones.
  const uint32_t ubo_offset = static_cast<uint32_t>(pass_cursor * ubo_stride);
  std::memcpy(frame.uniform_buffer_mapped + ubo_offset, &pass.view_data, sizeof(ViewData));
  std::memcpy(frame.quad_buffer_mapped + quad_cursor, quads.data(), sizeof(Quad) * quads.size());

  std::array<vk::DescriptorSet, 2> sets = {
    *frame.descriptor_set,                 // Set 0: UBO + Quad SSBO
    material_handler->get_descriptor_set() // Set 1: Bindless Textures + Material SSBO
  };

  for (const auto& batch : batches) {
    if (batch.quad_count == 0) continue;

    MaterialID batch_mat_id = batch_materials_pool[batch.first_material_offset];
    auto r_vert = material_pool->get_vertex_shader(batch_mat_id);
    auto r_frag = material_pool->get_fragment_shader(batch_mat_id);
    if (!r_vert || !r_frag) {
      std::cerr << "VulkanRenderer::end_pass failed to get shaders for batch" << std::endl;
      continue;
    }

    auto r_get_pipeline = pipeline_cache->get(r_vert.value(), r_frag.value(), pass.format);
    if (!r_get_pipeline) {
      std::cerr << "VulkanRenderer::end_pass failed to get pipeline: " << r_get_pipeline.error() << std::endl;
      continue;
    }

    VulkanPipeline* pipeline = r_get_pipeline.value();
    pipeline->bind(frame.command_buffer);

    frame.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->get_layout(), 0, sets, ubo_offset);

    const uint32_t first_vertex = static_cast<uint32_t>(quad_cursor + batch.first_quad) * 6;
    frame.command_buffer.draw(batch.quad_count * 6, 1, first_vertex, 0);
  }

  pass_cursor++;
  quad_cursor += quads.size();
}

std::vector<MaterialID> VulkanRenderer::get_needed_pass_materials() {
  std::vector<MaterialID> needed_materials;
  needed_materials.reserve(pending_commands.size());
  for (const auto& cmd : pending_commands) {
    needed_materials.push_back(cmd.material_id);
  }

  std::sort(needed_materials.begin(), needed_materials.end(),
            [](const MaterialID& a, const MaterialID& b) { return a.id < b.id; });
  needed_materials.erase(std::unique(needed_materials.begin(), needed_materials.end()), needed_materials.end());
  return needed_materials;
}

Result<void> VulkanRenderer::end_frame() {
  if (!in_frame) return mole::fail("end_frame called without begin_frame");
  if (active_pass) return mole::fail("end_frame called while a pass is open");

  FrameData& frame = frames[current_frame];
  in_frame = false;

  std::vector<vk::Semaphore> wait_semaphores;
  std::vector<vk::PipelineStageFlags> wait_stages;
  std::vector<vk::Semaphore> signal_semaphores;
  std::vector<vk::SwapchainKHR> swapchains;
  std::vector<uint32_t> image_indices;

  // 1. Transition every acquired backbuffer to present, then submit
  for (PresenterID presenter_id : acquired_presenters) {
    VulkanPresenter& presenter = *presenters.get(presenter_id)->presenter;
    vk::ImageLayout& layout = presenter.get_acquired_image_layout();
    transition_image_layout(frame.command_buffer, presenter.get_acquired_image(), layout,
                            vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {},
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eBottomOfPipe);
    layout = vk::ImageLayout::ePresentSrcKHR;

    wait_semaphores.push_back(*presenter.get_image_available(current_frame));
    wait_stages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    signal_semaphores.push_back(*presenter.get_render_finished());
    swapchains.push_back(presenter.get_swapchain());
    image_indices.push_back(presenter.get_acquired_image_index());
  }

  if (frame.command_buffer.end() != vk::Result::eSuccess) {
    return mole::fail("Failed to end the frame command buffer");
  }

  const vk::SubmitInfo submit_info{
    .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
    .pWaitSemaphores = wait_semaphores.data(),
    .pWaitDstStageMask = wait_stages.data(),
    .commandBufferCount = 1,
    .pCommandBuffers = &*frame.command_buffer,
    .signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
    .pSignalSemaphores = signal_semaphores.data()
  };

  if (vk_context->get_graphics_queue().submit(submit_info, *frame.in_flight_fence) != vk::Result::eSuccess) {
    return mole::fail("Failed to submit the frame (device lost?)");
  }

  // 2. Present every presenter drawn into this frame with one call
  std::optional<std::string> fatal_error;
  if (!swapchains.empty()) {
    std::vector<vk::Result> present_results(swapchains.size(), vk::Result::eSuccess);
    const vk::PresentInfoKHR present_info{
      .waitSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size()),
      .pWaitSemaphores = signal_semaphores.data(),
      .swapchainCount = static_cast<uint32_t>(swapchains.size()),
      .pSwapchains = swapchains.data(),
      .pImageIndices = image_indices.data(),
      .pResults = present_results.data()
    };

    const vk::Result present_result = vk_context->get_graphics_queue().presentKHR(present_info);
    for (size_t i = 0; i < acquired_presenters.size(); ++i) {
      switch (to_swapchain_status(present_results[i])) {
        case ESwapchainStatus::Ok:
          break;
        case ESwapchainStatus::Suboptimal:
        case ESwapchainStatus::OutOfDate:
          presenters.get(acquired_presenters[i])->presenter->mark_needs_recreate();
          break;
        case ESwapchainStatus::SurfaceLost:
          fatal_error = "Surface lost while presenting";
          break;
        case ESwapchainStatus::DeviceLost:
          fatal_error = "Device lost while presenting";
          break;
        case ESwapchainStatus::Error:
          fatal_error = "Failed to present";
          break;
      }
    }
    if (!fatal_error && to_swapchain_status(present_result) >= ESwapchainStatus::SurfaceLost) {
      fatal_error = "Failed to present";
    }
  }

  // 3. Advance the frame
  for (PresenterID presenter_id : acquired_presenters) {
    presenters.get(presenter_id)->presenter->release();
  }
  acquired_presenters.clear();

  frames_rendered->count++;
  material_handler->free_unused_materials();
  move_to_next_frame();

  if (fatal_error) return mole::fail(*fatal_error);
  return mole::ok();
}

Result<RenderTargetID> VulkanRenderer::create_render_target(const RenderTargetOptions& options) {
  MOLE_TRY_ASSIGN(std::unique_ptr<VulkanRenderTarget> render_target, VulkanRenderTarget::create(*vk_context, *memory_allocator, options));
  return render_targets.insert(RenderTargetEntry{std::move(render_target)});
}

Result<PresenterID> VulkanRenderer::create_presenter(const IWindow* window) {
  MOLE_TRY_ASSIGN(std::unique_ptr<VulkanPresenter> presenter, VulkanPresenter::create(*vk_context, window));

  const PresenterID presenter_id = presenters.insert(PresenterEntry{
    .presenter = std::move(presenter),
    .render_target = RenderTargetID::INVALID
  });
  presenters.get(presenter_id)->render_target = render_targets.insert(RenderTargetEntry{presenter_id});
  return presenter_id;
}

Result<void> VulkanRenderer::resize(RenderTargetID target, uint32_t width, uint32_t height) {
  RenderTargetEntry* entry = render_targets.get(target);
  if (!entry) return mole::fail("resize called with an invalid or stale RenderTargetID");

  auto* offscreen = std::get_if<std::unique_ptr<VulkanRenderTarget>>(entry);
  if (!offscreen) return mole::fail("Presenter render targets can't be resized manually");
  if (active_pass && active_pass->target == target) {
    return mole::fail("resize called on the render target of the open pass");
  }

  const RenderTargetOptions options{
    .width = width,
    .height = height,
    .format = (*offscreen)->get_options_format()
  };
  MOLE_TRY_ASSIGN(std::unique_ptr<VulkanRenderTarget> resized, VulkanRenderTarget::create(*vk_context, *memory_allocator, options));

  retire(std::exchange(*offscreen, std::move(resized)));
  return mole::ok();
}

Result<void> VulkanRenderer::destroy(RenderTargetID target) {
  RenderTargetEntry* entry = render_targets.get(target);
  if (!entry) return mole::fail("destroy called with an invalid or stale RenderTargetID");
  if (!std::holds_alternative<std::unique_ptr<VulkanRenderTarget>>(*entry)) {
    return mole::fail("Presenter render targets are destroyed together with their presenter");
  }
  if (active_pass && active_pass->target == target) {
    return mole::fail("destroy called on the render target of the open pass");
  }

  std::optional<RenderTargetEntry> erased = render_targets.erase(target);
  retire(std::get<std::unique_ptr<VulkanRenderTarget>>(std::move(*erased)));
  return mole::ok();
}

Result<void> VulkanRenderer::destroy(PresenterID presenter) {
  PresenterEntry* entry = presenters.get(presenter);
  if (!entry) return mole::fail("destroy called with an invalid or stale PresenterID");
  if (entry->presenter->is_acquired()) {
    return mole::fail("destroy called on a presenter that was drawn into this frame");
  }

  if (vk_context->get_logical_device().waitIdle() != vk::Result::eSuccess) {
    return mole::fail("Failed to wait for device idle before destroying a presenter");
  }

  (void)render_targets.erase(entry->render_target);
  (void)presenters.erase(presenter);
  return mole::ok();
}

void VulkanRenderer::retire(std::unique_ptr<VulkanRenderTarget> render_target) {
  retired_render_targets.push_back(RetiredRenderTarget{
    .retire_after_frame = frames_rendered->count + MAX_FRAMES_IN_FLIGHT,
    .render_target = std::move(render_target)
  });
}

RenderTargetID VulkanRenderer::get_render_target(PresenterID presenter) const {
  const PresenterEntry* entry = presenters.get(presenter);
  return entry ? entry->render_target : RenderTargetID::INVALID;
}

Result<RenderTargetInfo> VulkanRenderer::get_info(RenderTargetID target) const {
  const RenderTargetEntry* entry = render_targets.get(target);
  if (!entry) return mole::fail("Invalid or stale RenderTargetID");

  vk::Extent2D extent;
  vk::Format format;
  if (const auto* offscreen = std::get_if<std::unique_ptr<VulkanRenderTarget>>(entry)) {
    extent = (*offscreen)->get_extent();
    format = (*offscreen)->get_format();
  } else {
    const VulkanPresenter& presenter = *presenters.get(std::get<PresenterID>(*entry))->presenter;
    extent = presenter.get_extent();
    format = presenter.get_format();
  }

  RenderTargetInfo info{
    .width = extent.width,
    .height = extent.height,
    .native_format = static_cast<uint32_t>(format)
  };
  return info;
}

Result<PresenterInfo> VulkanRenderer::get_info(PresenterID presenter) const {
  const PresenterEntry* entry = presenters.get(presenter);
  if (!entry) return mole::fail("Invalid or stale PresenterID");

  const VulkanPresenter& vk_presenter = *entry->presenter;
  PresenterInfo info{
    .width = vk_presenter.get_extent().width,
    .height = vk_presenter.get_extent().height,
    .image_count = vk_presenter.get_image_count(),
    .native_format = static_cast<uint32_t>(vk_presenter.get_format())
  };
  return info;
}

void VulkanRenderer::transition_image_layout(vk::raii::CommandBuffer& cmd_buffer, vk::Image image,
                                             vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                                             vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
                                             vk::PipelineStageFlags2 src_stage_mask,
                                             vk::PipelineStageFlags2 dst_stage_mask) {
  vk::ImageMemoryBarrier2 barrier{
    .srcStageMask = src_stage_mask,
    .srcAccessMask = src_access_mask,
    .dstStageMask = dst_stage_mask,
    .dstAccessMask = dst_access_mask,
    .oldLayout = old_layout,
    .newLayout = new_layout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = {
      .aspectMask = vk::ImageAspectFlagBits::eColor,
      .levelCount = 1,
      .layerCount = 1
    }
  };

  vk::DependencyInfo dependency_info{
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier
  };

  cmd_buffer.pipelineBarrier2(dependency_info);
}

inline void VulkanRenderer::move_to_next_frame() { current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT; }

void* VulkanRenderer::get_native_handle(ERendererNativeHandle type) const {
  switch (type) {
    case ERendererNativeHandle::Instance:
      return to_native_handle_ptr(static_cast<VkInstance>(*vk_context->get_instance()));
    case ERendererNativeHandle::PhysicalAdapter:
      return to_native_handle_ptr(static_cast<VkPhysicalDevice>(*vk_context->get_physical_device()));
    case ERendererNativeHandle::Device:
      return to_native_handle_ptr(static_cast<VkDevice>(*vk_context->get_logical_device()));
    case ERendererNativeHandle::ImmediateContext:
      return nullptr;
    case ERendererNativeHandle::GraphicsQueue:
    case ERendererNativeHandle::ComputeQueue:
    case ERendererNativeHandle::TransferQueue:
      return to_native_handle_ptr(static_cast<VkQueue>(*vk_context->get_graphics_queue()));
    case ERendererNativeHandle::ActiveCommandBuffer:
      return to_native_handle_ptr(static_cast<VkCommandBuffer>(*frames[current_frame].command_buffer));
    default:
      return nullptr;
  }
}

void* VulkanRenderer::get_native_handle(RenderTargetID target, ERenderTargetNativeHandle type) const {
  const RenderTargetEntry* entry = render_targets.get(target);
  if (!entry) return nullptr;

  vk::Image image;
  vk::ImageView image_view;
  if (const auto* offscreen = std::get_if<std::unique_ptr<VulkanRenderTarget>>(entry)) {
    image = (*offscreen)->get_image();
    image_view = (*offscreen)->get_image_view();
  } else {
    // Null unless the backbuffer was acquired this frame
    const VulkanPresenter& presenter = *presenters.get(std::get<PresenterID>(*entry))->presenter;
    image = presenter.get_acquired_image();
    image_view = presenter.get_acquired_image_view();
  }

  switch (type) {
    case ERenderTargetNativeHandle::Image:
      return to_native_handle_ptr(static_cast<VkImage>(image));
    case ERenderTargetNativeHandle::ImageView:
      return to_native_handle_ptr(static_cast<VkImageView>(image_view));
    default:
      return nullptr;
  }
}

void* VulkanRenderer::get_native_handle(PresenterID presenter) const {
  const PresenterEntry* entry = presenters.get(presenter);
  if (!entry) return nullptr;
  return to_native_handle_ptr(static_cast<VkSwapchainKHR>(entry->presenter->get_swapchain()));
}

RendererBackendInfo VulkanRenderer::get_backend_info() const {
  const uint32_t queue_family = vk_context->get_graphics_queue_index();

  RendererBackendInfo info{
    .graphics_queue_family = queue_family,
    .compute_queue_family = queue_family,
    .transfer_queue_family = queue_family,
    .frames_in_flight = MAX_FRAMES_IN_FLIGHT
  };

  return info;
}

void VulkanRenderer::build_batches() {
  // 1. Clear GPU-bound structures (clears count, but keeps allocated capacity)
  quads.clear();
  batches.clear();
  batch_materials_pool.clear();

  if (pending_commands.empty()) return;

  // 2. Sort all commands for the frame
  std::stable_sort(
      pending_commands.begin(), pending_commands.end(), [this](const RenderQuadCommand& a, const RenderQuadCommand& b) {
        if (a.z_index != b.z_index) {
          return a.z_index < b.z_index; // Primary: Z-Index (Back to front)
        }

        auto r_va = material_pool->get_vertex_shader(a.material_id);
        auto r_fa = material_pool->get_fragment_shader(a.material_id);
        auto r_vb = material_pool->get_vertex_shader(b.material_id);
        auto r_fb = material_pool->get_fragment_shader(b.material_id);

        CombinedShaderKey shaders_a{r_va ? r_va.value().get_packed_id() : 0, r_fa ? r_fa.value().get_packed_id() : 0};
        CombinedShaderKey shaders_b{r_vb ? r_vb.value().get_packed_id() : 0, r_fb ? r_fb.value().get_packed_id() : 0};

        if (shaders_a != shaders_b) {
          return shaders_a < shaders_b; // Secondary: Shader ID
        }

        // Tertiary: Material ID (using the underlying 64-bit int for grouping)
        return a.material_id.id < b.material_id.id;
      });

  // 3. Pre-allocate quad memory to prevent reallocations
  quads.reserve(pending_commands.size());

  std::optional<CombinedShaderKey> current_shader;

  // 4. Build batches and quads
  for (const auto& cmd : pending_commands) {
    // Resolve Material
    auto r_get_mat_idx = material_handler->get_material_index(cmd.material_id);
    if (!r_get_mat_idx) {
      std::cerr << "VulkanRenderer::build_batches failed to get material index for material " << cmd.material_id.id
                << ": " << r_get_mat_idx.error() << std::endl;
      continue;
    }
    uint32_t gpu_material_index = r_get_mat_idx.value();

    auto r_vert = material_pool->get_vertex_shader(cmd.material_id);
    auto r_frag = material_pool->get_fragment_shader(cmd.material_id);
    CombinedShaderKey shader_key{r_vert ? r_vert.value().get_packed_id() : 0,
                                 r_frag ? r_frag.value().get_packed_id() : 0};

    // Check if we need a new batch (only breaks on shader change)
    if (!current_shader.has_value() || shader_key != current_shader.value()) {
      RenderBatch new_batch;
      new_batch.shader_key = shader_key;
      new_batch.first_quad = static_cast<uint32_t>(quads.size());
      new_batch.quad_count = 0;

      new_batch.first_material_offset = static_cast<uint32_t>(batch_materials_pool.size());
      new_batch.material_count = 0;

      batches.push_back(new_batch);
      current_shader = shader_key;
    }

    RenderBatch& active_batch = batches.back();

    // 5. Track unique materials for this batch using the actual MaterialID
    auto mat_start = batch_materials_pool.begin() + active_batch.first_material_offset;
    auto mat_end = batch_materials_pool.end();

    // Find the material in the current batch's tracked list
    auto it = std::find(mat_start, mat_end, cmd.material_id);

    // Calculate the LOCAL index for the shader (0, 1, 2...)
    uint32_t local_mat_idx = static_cast<uint32_t>(std::distance(mat_start, it));

    // If it wasn't found, add it to the pool
    if (it == mat_end) {
      batch_materials_pool.push_back(cmd.material_id);
      active_batch.material_count++;
    }

    // 6. Build the quad
    Quad quad;
    quad.transform[0] = glm::vec2(cmd.transform[0]);
    quad.transform[1] = glm::vec2(cmd.transform[1]);
    quad.transform[2] = glm::vec2(cmd.transform[2]);

    // Important: Assign the GLOBAL GPU slot index so the shader looks up the right material
    quad.material_index = gpu_material_index;
    quad._padding = 0;

    quads.push_back(quad);
    active_batch.quad_count++;
  }

  // 7. Clear pending commands so they don't bleed into the next frame
  pending_commands.clear();
}

} // mole