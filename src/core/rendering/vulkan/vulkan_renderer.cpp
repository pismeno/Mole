#include "rendering/vulkan/vulkan_renderer.hpp"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>
#include <memory>
#include <iostream>

#include <rendering/vulkan/vulkan_context.hpp>
#include <rendering/vulkan/vulkan_memory_allocator.hpp>
#include <rendering/vulkan/vulkan_swapchain.hpp>
#include <rendering/vulkan/vulkan_pipeline.hpp>
#include <utils/result.hpp>

namespace mole {

VulkanRenderer::~VulkanRenderer() {
  if (!is_context_lost_ && vk_context) {
    vk_context->get_logical_device().waitIdle();
  }
}

Result<std::unique_ptr<VulkanRenderer>> VulkanRenderer::create(RenderingContext& context, void* native_window_handle, uint32_t window_width, uint32_t window_height) {
  std::unique_ptr<VulkanRenderer> renderer(new VulkanRenderer());

  renderer->asset_manager = context.resources_context->asset_manager;
  renderer->material_pool = context.resources_context->material_pool;
  renderer->frames_rendered = std::make_unique<Counter>();

  // 1. Initialize Subsystems
  auto r_context = VulkanContext::create(native_window_handle);
  if (!r_context) return mole::fail(r_context.error());
  renderer->vk_context = std::move(r_context).value();
  std::cout << "Vulkan Context created successfully" << std::endl;

  auto r_allocator = VulkanMemoryAllocator::create(*renderer->vk_context);
  if (!r_allocator) return mole::fail(r_allocator.error());
  renderer->memory_allocator = std::move(r_allocator).value();
  std::cout << "VMA Allocator created successfully" << std::endl;

  auto r_swapchain = VulkanSwapchain::create(*renderer->vk_context, window_width, window_height);
  if (!r_swapchain) return mole::fail(r_swapchain.error());
  renderer->swapchain = std::move(r_swapchain).value();
  std::cout << "Swapchain created successfully" << std::endl;

  // 2. Initialize Internal Renderer State
  auto r_create_command_pool = renderer->create_command_pool();
  if (!r_create_command_pool) return mole::fail(r_create_command_pool.error());

  auto r_material_handler = VulkanMaterialHandler::create(*renderer->vk_context, *renderer->memory_allocator, *renderer->frames_rendered, context);
  if (!r_material_handler) return mole::fail(r_material_handler.error());
  renderer->material_handler = std::move(r_material_handler).value();
  std::cout << "Material Handler created successfully" << std::endl;

  auto r_create_desc_set_layot = renderer->create_descriptor_set_layout();
  if (!r_create_desc_set_layot) return mole::fail(r_create_desc_set_layot.error());
  std::cout << "Descriptor set layout created successfully" << std::endl;

  renderer->pipeline_cache = std::make_unique<VulkanPipelineCache>(*renderer->swapchain, *renderer->vk_context, *renderer->material_handler, renderer->descriptor_set_layout);
  std::cout << "Pipeline created successfully" << std::endl;

  auto r_create_frame_data = renderer->create_frame_data();
  if (!r_create_frame_data) return mole::fail(r_create_frame_data.error());

  std::cout << "Vulkan Renderer initialized successfully" << std::endl;

  return std::move(renderer);
}

Result<void> VulkanRenderer::create_command_pool() {
  vk::CommandPoolCreateInfo pool_info{
      .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
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
      .binding            = 0,
      .descriptorType     = vk::DescriptorType::eUniformBuffer,
      .descriptorCount    = 1,
      .stageFlags         = vk::ShaderStageFlagBits::eVertex,
      .pImmutableSamplers = nullptr
  };

  vk::DescriptorSetLayoutBinding ssbo_binding = {
      .binding = 1,
      .descriptorType = vk::DescriptorType::eStorageBuffer, // <--- Storage Buffer!
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex
  };

  std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {ubo_binding, ssbo_binding};

  vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data()
  };

  auto vkr_create_desc_set_layout = vk_context->get_logical_device().createDescriptorSetLayout(descriptor_set_layout_info);
  if (vkr_create_desc_set_layout.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create descriptor set layout");
  }

  descriptor_set_layout = std::move(vkr_create_desc_set_layout.value);
  return mole::ok();
}

Result<void> VulkanRenderer::create_frame_data() {
  frames.resize(MAX_FRAMES_IN_FLIGHT);

  // 1. Create Descriptor Pool for UBOs (Set 0)
  std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
      vk::DescriptorPoolSize{
          .type            = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = MAX_FRAMES_IN_FLIGHT
      },
      vk::DescriptorPoolSize{
          .type            = vk::DescriptorType::eStorageBuffer, // <--- Add this!
          .descriptorCount = MAX_FRAMES_IN_FLIGHT
      }
  };

  vk::DescriptorPoolCreateInfo pool_info{
      .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets       = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes    = pool_sizes.data()
  };

  auto vkr_ubo_pool = vk_context->get_logical_device().createDescriptorPool(pool_info);
  if (vkr_ubo_pool.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create UBO descriptor pool");
  }
  ubo_descriptor_pool = std::move(vkr_ubo_pool.value);

  // 2. Allocate Command Buffers
  vk::CommandBufferAllocateInfo alloc_info{
      .commandPool        = *command_pool,
      .level              = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT
  };

  auto vkr_cmd_buffers = vk_context->get_logical_device().allocateCommandBuffers(alloc_info);
  if (vkr_cmd_buffers.result != vk::Result::eSuccess) {
    return mole::fail("Failed to allocate command buffers");
  }

  // 3. Initialize Per-Frame Data
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    FrameData& frame = frames[i];

    // Sync Objects
    auto vkr_image_avail = vk_context->get_logical_device().createSemaphore(vk::SemaphoreCreateInfo());
    if (vkr_image_avail.result != vk::Result::eSuccess) return mole::fail("Failed to create semaphore");
    frame.image_available_semaphore = std::move(vkr_image_avail.value);

    auto vkr_fence = vk_context->get_logical_device().createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    if (vkr_fence.result != vk::Result::eSuccess) return mole::fail("Failed to create fence");
    frame.in_flight_fence = std::move(vkr_fence.value);

    // Command Buffer
    frame.command_buffer = std::move(vkr_cmd_buffers.value[i]);

    // Uniform Buffer
    auto ubo_res = memory_allocator->create_buffer(vk::BufferUsageFlagBits::eUniformBuffer, sizeof(ViewData));
    if (!ubo_res) return mole::fail(ubo_res.error());
    frame.uniform_buffer = std::move(ubo_res).value();
    frame.uniform_buffer_mapped = frame.uniform_buffer.getAllocation().getInfo().pMappedData;

    auto r_create_quad_buf = memory_allocator->create_buffer(vk::BufferUsageFlagBits::eStorageBuffer, sizeof(Quad) * MAX_QUADS);
    if (!r_create_quad_buf) return mole::fail(r_create_quad_buf.error());
    frame.quad_buffer = std::move(r_create_quad_buf).value();
    frame.quad_buffer_mapped = static_cast<Quad*>(frame.quad_buffer.getAllocation().getInfo().pMappedData);

    // Allocate Set 0 for this frame
    vk::DescriptorSetLayout raw_ubo_layout = *descriptor_set_layout;
    vk::DescriptorSetAllocateInfo desc_alloc_info{
        .descriptorPool     = *ubo_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &raw_ubo_layout
    };

    auto vkr_desc_set = vk_context->get_logical_device().allocateDescriptorSets(desc_alloc_info);
    if (vkr_desc_set.result != vk::Result::eSuccess) {
      return mole::fail("Failed to allocate frame UBO descriptor set");
    }
    frame.descriptor_set = std::move(vkr_desc_set.value[0]);

    // Point Set 0 to this frame's uniform buffer
    vk::DescriptorBufferInfo ubo_info{
        .buffer = *frame.uniform_buffer,
        .offset = 0,
        .range  = sizeof(ViewData)
    };

    vk::DescriptorBufferInfo ssbo_info{
        .buffer = *frame.quad_buffer,
        .offset = 0,
        .range  = sizeof(Quad) * MAX_QUADS
    };

    std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {
        vk::WriteDescriptorSet{
            .dstSet          = *frame.descriptor_set,
            .dstBinding      = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo     = &ubo_info
        },
        vk::WriteDescriptorSet{
            .dstSet          = *frame.descriptor_set,
            .dstBinding      = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo     = &ssbo_info
        }
    };

    vk_context->get_logical_device().updateDescriptorSets(descriptor_writes, nullptr);
  }

  // 4. Render Finished Semaphores (Tied to Swapchain image count)
  render_finished_semaphores.clear();
  for (size_t i = 0; i < swapchain->get_image_count(); i++) {
    auto vkr_render_fin = vk_context->get_logical_device().createSemaphore(vk::SemaphoreCreateInfo());
    if (vkr_render_fin.result != vk::Result::eSuccess) return mole::fail("Failed to create render finished semaphore");
    render_finished_semaphores.push_back(std::move(vkr_render_fin.value));
  }

  return mole::ok();
}

void VulkanRenderer::begin_frame() {
  if (needs_framebuffer_resize_ || is_context_lost_) return;

  FrameData& frame = frames[current_frame_];

  // 1. Wait for the GPU to finish with this frame's resources
  auto vkr_fence = vk_context->get_logical_device().waitForFences(*frame.in_flight_fence, vk::True, UINT64_MAX);
  if (vkr_fence == vk::Result::eErrorDeviceLost) {
    is_context_lost_ = true;
    return;
  }
  vk_context->get_logical_device().resetFences(*frame.in_flight_fence);

  // 2. Acquire the next image to draw onto (Your "target_texture" logic)
  auto r_acquire = swapchain->acquire_next_image(frame.image_available_semaphore);
  if (!r_acquire) {
    needs_framebuffer_resize_ = true;
    return;
  }
  current_image_index_ = r_acquire.value();

  // 3. Begin the Vulkan Command Buffer for this frame
  if (frame.command_buffer.begin({}) != vk::Result::eSuccess) {
    std::cerr << "Failed to begin command buffer\n";
    return;
  }

  // 4. Transition the target texture so we can render to it
  transition_image_layout(frame.command_buffer, swapchain->get_images()[current_image_index_],
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                          {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
}

void VulkanRenderer::begin_pass(const ViewData& view_data) {
  if (needs_framebuffer_resize_ || is_context_lost_) return;
  current_view_data_ = view_data;
}

void VulkanRenderer::submit(const RenderQuadCommand render_command) {
  pending_commands.push_back(render_command);
}

void VulkanRenderer::submit(const std::vector<RenderQuadCommand>& render_commands) {
  pending_commands.insert(pending_commands.end(),
                          render_commands.begin(),
                          render_commands.end());
}

void VulkanRenderer::end_pass() {
  if (needs_framebuffer_resize_ || is_context_lost_ || swapchain->get_extent().width == 0 || swapchain->get_extent().height == 0) {
    pending_commands.clear();
    return;
  }
  if (pending_commands.empty()) return;

  FrameData& frame = frames[current_frame_];

  // --- 1. CPU PHASE: FLUSH THE COMMAND QUEUE ---
  std::vector<MaterialID> pass_materials = get_needed_pass_materials();
  auto r_prepare_materials = material_handler->prepare_materials(pass_materials);
  if (!r_prepare_materials) {
    std::cerr << "VulkanRenderer::end_pass failed to prepare materials: " << r_prepare_materials.error() << std::endl;
  }

  // Build batches (This internally calls pending_commands.clear(), giving you a fresh queue)
  build_batches();

  // Update CPU-mapped GPU buffers
  update_ubo(current_view_data_, frame);
  update_quad_ssbo(frame);

  // --- 2. GPU PHASE: RECORD RENDER COMMANDS ---

  vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 1.0f, 0.5f, 1.0f);
  vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView   = *swapchain->get_image_views()[current_image_index_], // Or your custom target_texture View
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp      = vk::AttachmentLoadOp::eClear,
      .storeOp     = vk::AttachmentStoreOp::eStore,
      .clearValue  = clearColor};

  vk::RenderingInfo renderingInfo = {
      .renderArea           = {.offset = {0, 0}, .extent = swapchain->get_extent()},
      .layerCount           = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments    = &attachmentInfo};

  frame.command_buffer.beginRendering(renderingInfo);

  frame.command_buffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapchain->get_extent().height), static_cast<float>(swapchain->get_extent().width), -static_cast<float>(swapchain->get_extent().height), 0.0f, 1.0f));
  frame.command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain->get_extent()));

  std::array<vk::DescriptorSet, 2> sets = {
      *frame.descriptor_set,                 // Set 0: UBO + Quad SSBO
      material_handler->get_descriptor_set() // Set 1: Bindless Textures + Material SSBO
  };

  // Issue draw calls from batches
  for (const auto& batch : batches) {
    if (batch.quad_count == 0) continue;

    MaterialID batch_mat_id = batch_materials_pool[batch.first_material_offset];
    const Material& mat = material_pool->get(batch_mat_id);

    auto r_get_pipeline = pipeline_cache->get(mat.vertex_shader, mat.fragment_shader);
    if (!r_get_pipeline) {
      std::cerr << "VulkanRenderer::end_pass failed to get pipeline: " << r_get_pipeline.error() << std::endl;
      continue;
    }

    VulkanPipeline* pipeline = r_get_pipeline.value();
    pipeline->bind(frame.command_buffer);

    frame.command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipeline->get_layout(),
        0, sets, nullptr
    );

    frame.command_buffer.draw(batch.quad_count * 6, 1, batch.first_quad * 6, 0);
  }

  frame.command_buffer.endRendering();
}

std::vector<MaterialID> VulkanRenderer::get_needed_pass_materials() {
  std::vector<MaterialID> needed_materials;
  needed_materials.reserve(this->pending_commands.size());
  for (const auto& cmd : this->pending_commands) {
    needed_materials.push_back(cmd.material_id);
  }

  std::sort(needed_materials.begin(), needed_materials.end(), [](const MaterialID& a, const MaterialID& b) {
    return a.id < b.id;
  });
  needed_materials.erase(std::unique(needed_materials.begin(), needed_materials.end()), needed_materials.end());
  return std::move(needed_materials);
}

void VulkanRenderer::end_frame() {
  if (needs_framebuffer_resize_ || is_context_lost_) return;

  FrameData& frame = frames[current_frame_];

  // 1. Transition image to presentation layout
  transition_image_layout(frame.command_buffer, swapchain->get_images()[current_image_index_],
                          vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite, {},
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);

  // 2. End the command buffer
  if (frame.command_buffer.end() != vk::Result::eSuccess) {
    std::cerr << "Failed to end command buffer\n";
    return;
  }

  // 3. Submit the frame to the GPU queue
  vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount   = 1,
      .pWaitSemaphores      = &*frame.image_available_semaphore,
      .pWaitDstStageMask    = &waitDestinationStageMask,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &*frame.command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = &*render_finished_semaphores[current_image_index_]
  };

  vk_context->get_graphics_queue().submit(submitInfo, *frame.in_flight_fence);

  // 4. Present it to the screen
  auto r_present = swapchain->present(vk_context->get_graphics_queue(), render_finished_semaphores[current_image_index_], current_image_index_);
  if (!r_present) {
    needs_framebuffer_resize_ = true;
  }

  frames_rendered->count++;
  material_handler->free_unused_materials();
  move_to_next_frame();
}

void VulkanRenderer::update_ubo(const ViewData& view_data, FrameData& frame) {
  memcpy(frame.uniform_buffer_mapped, &view_data, sizeof(view_data));
}

void VulkanRenderer::update_quad_ssbo(FrameData& frame) {
  std::memcpy(frame.quad_buffer_mapped, quads.data(), sizeof(mole::Quad) * quads.size());
}

Result<void> VulkanRenderer::resize_framebuffer(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    needs_framebuffer_resize_ = true;
    return mole::ok();
  }

  const auto& physical_device = vk_context->get_physical_device();
  const auto& surface = vk_context->get_surface();
  auto caps_res = physical_device.getSurfaceCapabilitiesKHR(*surface);
  if (caps_res.result == vk::Result::eSuccess) {
    if (caps_res.value.currentExtent.width == 0 && caps_res.value.currentExtent.height == 0) {
      needs_framebuffer_resize_ = true;
      return mole::ok();
    }
  }

  vk_context->get_logical_device().waitIdle();

  auto r_recreate = swapchain->recreate(*vk_context, width, height);
  if (!r_recreate) {
    needs_framebuffer_resize_ = true;
    return r_recreate;
  }

  render_finished_semaphores.clear();
  for (size_t i = 0; i < swapchain->get_image_count(); i++) {
    auto vkr_render_fin = vk_context->get_logical_device().createSemaphore(vk::SemaphoreCreateInfo());
    if (vkr_render_fin.result != vk::Result::eSuccess) return mole::fail("Failed to create semaphore");
    render_finished_semaphores.push_back(std::move(vkr_render_fin.value));
  }

  needs_framebuffer_resize_ = false;
  return mole::ok();
}

void VulkanRenderer::transition_image_layout(
    vk::raii::CommandBuffer& cmd_buffer, vk::Image image,
    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask)
{
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask        = src_stage_mask,
      .srcAccessMask       = src_access_mask,
      .dstStageMask        = dst_stage_mask,
      .dstAccessMask       = dst_access_mask,
      .oldLayout           = old_layout,
      .newLayout           = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image               = image,
      .subresourceRange    = { .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 }
  };

  vk::DependencyInfo dependency_info = {
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers    = &barrier
  };

  cmd_buffer.pipelineBarrier2(dependency_info);
}

inline void VulkanRenderer::move_to_next_frame() {
  current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanRenderer::is_context_lost() const noexcept { return is_context_lost_; }
bool VulkanRenderer::needs_framebuffer_resize() const noexcept { return needs_framebuffer_resize_; }

void VulkanRenderer::build_batches() {
  // 1. Clear GPU-bound structures (clears count, but keeps allocated capacity)
  quads.clear();
  batches.clear();
  batch_materials_pool.clear();

  if (pending_commands.empty()) return;

  // 2. Sort all commands for the frame
  std::stable_sort(pending_commands.begin(), pending_commands.end(),
                   [this](const RenderQuadCommand& a, const RenderQuadCommand& b) {
                     if (a.z_index != b.z_index) {
                       return a.z_index < b.z_index; // Primary: Z-Index (Back to front)
                     }

                     const Material& mat_a = material_pool->get(a.material_id);
                     const Material& mat_b = material_pool->get(b.material_id);

                     CombinedShaderKey shaders_a{ mat_a.vertex_shader.get_packed_id(), mat_a.fragment_shader.get_packed_id() };
                     CombinedShaderKey shaders_b{ mat_b.vertex_shader.get_packed_id(), mat_b.fragment_shader.get_packed_id() };

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
      std::cerr << "VulkanRenderer::build_batches failed to get material index for material " << cmd.material_id.id << ": " << r_get_mat_idx.error() << std::endl;
      continue;
    }
    uint32_t gpu_material_index = r_get_mat_idx.value();

    const Material& mat = material_pool->get(cmd.material_id);
    CombinedShaderKey shader_key{ mat.vertex_shader.get_packed_id(), mat.fragment_shader.get_packed_id() };

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