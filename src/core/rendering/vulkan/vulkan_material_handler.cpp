#include "rendering/vulkan/vulkan_material_handler.hpp"

#include <cstring>
#include <vector>
#include <unordered_set>

namespace mole {

Result<std::unique_ptr<VulkanMaterialHandler>> VulkanMaterialHandler::create(
    const VulkanContext& vk_context,
    VulkanMemoryAllocator& memory_allocator,
    const Counter& frame_counter,
    const RenderingContext& context)
{
  std::unique_ptr<VulkanMaterialHandler> handler(new VulkanMaterialHandler(vk_context, memory_allocator, frame_counter, context));

  auto r_layout = handler->create_descriptor_set_layouts();
  if (!r_layout) return mole::fail(r_layout.error());

  auto r_pool = handler->create_descriptor_pool();
  if (!r_pool) return mole::fail(r_pool.error());

  auto r_set = handler->create_descriptor_set();
  if (!r_set) return mole::fail(r_set.error());

  auto r_sampler = handler->create_default_sampler();
  if (!r_sampler) return mole::fail(r_sampler.error());

  auto r_cmd_pool = handler->create_command_pool();
  if (!r_cmd_pool) return mole::fail(r_cmd_pool.error());

  auto r_mat_buffer = handler->create_material_buffer();
  if (!r_mat_buffer) return mole::fail(r_mat_buffer.error());

  return std::move(handler);
}

Result<void> VulkanMaterialHandler::create_descriptor_set_layouts() {
  // Binding 0: Bindless Textures
  vk::DescriptorSetLayoutBinding tex_binding{
      .binding         = 0,
      .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
      .descriptorCount = vk_context.get_texture_capacity(),
      .stageFlags      = vk::ShaderStageFlagBits::eFragment
  };

  // Binding 1: Material SSBO
  vk::DescriptorSetLayoutBinding mat_binding{
      .binding         = 1,
      .descriptorType  = vk::DescriptorType::eStorageBuffer,
      .descriptorCount = 1,
      .stageFlags      = vk::ShaderStageFlagBits::eFragment
  };

  std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {tex_binding, mat_binding};

  // Bindless flags for the textures, standard flags (0) for the SSBO
  std::array<vk::DescriptorBindingFlags, 2> binding_flags = {
      vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound,
      vk::DescriptorBindingFlags{}
  };

  vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{
      .bindingCount  = static_cast<uint32_t>(binding_flags.size()),
      .pBindingFlags = binding_flags.data()
  };

  vk::DescriptorSetLayoutCreateInfo create_info{
      .pNext        = &binding_flags_info,
      .flags        = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings    = bindings.data()
  };

  auto vkr_create = vk_context.get_logical_device().createDescriptorSetLayout(create_info);
  if (vkr_create.result != vk::Result::eSuccess) return mole::fail("Failed to create descriptor set layout");

  descriptor_set_layout = std::move(vkr_create.value);
  return mole::ok();
}

Result<void> VulkanMaterialHandler::create_descriptor_pool() {
  std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
      vk::DescriptorPoolSize{
          .type            = vk::DescriptorType::eCombinedImageSampler,
          .descriptorCount = vk_context.get_texture_capacity()
      },
      vk::DescriptorPoolSize{
          .type            = vk::DescriptorType::eStorageBuffer, // <--- Add this!
          .descriptorCount = 1
      }
  };

  vk::DescriptorPoolCreateInfo create_info{
      .flags         = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                       vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets       = pool_sizes.size(),
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes    = pool_sizes.data()
  };

  auto vkr_create = vk_context.get_logical_device().createDescriptorPool(create_info);
  if (vkr_create.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create descriptor pool");
  }
  descriptor_pool = std::move(vkr_create.value);
  return mole::ok();
}

Result<void> VulkanMaterialHandler::create_descriptor_set() {
  vk::DescriptorSetLayout raw_layout = *descriptor_set_layout;

  vk::DescriptorSetAllocateInfo alloc_info{
      .descriptorPool     = *descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts        = &raw_layout
  };

  auto vkr_create = vk_context.get_logical_device().allocateDescriptorSets(alloc_info);
  if (vkr_create.result != vk::Result::eSuccess) return mole::fail("Failed to allocate descriptor set");

  descriptor_set = std::move(vkr_create.value[0]);
  return mole::ok();
}

Result<void> VulkanMaterialHandler::create_default_sampler() {
  vk::SamplerCreateInfo sampler_info{
      .magFilter     = vk::Filter::eLinear,
      .minFilter     = vk::Filter::eLinear,
      .mipmapMode    = vk::SamplerMipmapMode::eLinear,
      .addressModeU  = vk::SamplerAddressMode::eRepeat,
      .addressModeV  = vk::SamplerAddressMode::eRepeat,
      .addressModeW  = vk::SamplerAddressMode::eRepeat,
      .maxAnisotropy = 1.0f,
      .borderColor   = vk::BorderColor::eIntOpaqueBlack
  };

  auto vkr_sampler = vk_context.get_logical_device().createSampler(sampler_info);
  if (vkr_sampler.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create default sampler");
  }
  default_sampler = std::move(vkr_sampler.value);
  return mole::ok();
}

Result<void> VulkanMaterialHandler::create_command_pool() {
  vk::CommandPoolCreateInfo pool_info{
      .flags            = vk::CommandPoolCreateFlagBits::eTransient,
      .queueFamilyIndex = vk_context.get_graphics_queue_index()
  };

  auto vkr_pool = vk_context.get_logical_device().createCommandPool(pool_info);
  if (vkr_pool.result != vk::Result::eSuccess) {
    return mole::fail("Failed to create transient command pool for texture uploads");
  }
  command_pool = std::move(vkr_pool.value);
  return mole::ok();
}

Result<void> VulkanMaterialHandler::create_material_buffer() {
  size_t total_buffer_size = MAX_MATERIALS * MATERIAL_STRIDE;

  auto r_buf = memory_allocator.create_buffer(vk::BufferUsageFlagBits::eStorageBuffer, total_buffer_size);
  if (!r_buf) return mole::fail(r_buf.error());

  material_buffer = std::move(r_buf).value();
  material_buffer_mapped = material_buffer.getAllocation().getInfo().pMappedData;

  // 2. Link this buffer to Binding 1 in our Descriptor Set
  vk::DescriptorBufferInfo buffer_info{
      .buffer = *material_buffer,
      .offset = 0,
      .range  = total_buffer_size
  };

  vk::WriteDescriptorSet write_desc{
      .dstSet          = *descriptor_set,
      .dstBinding      = 1, // <--- Matches the shader!
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType  = vk::DescriptorType::eStorageBuffer,
      .pBufferInfo     = &buffer_info
  };

  vk_context.get_logical_device().updateDescriptorSets(write_desc, nullptr);

  return mole::ok();
}

Result<void> VulkanMaterialHandler::update_texture_buffer() {
  std::vector<AssetHandle<ImageAsset>> pending = bindless_texture_registry->get_pending_texture_uploads();
  if (pending.empty()) {
    return mole::ok();
  }

  // 1. Compute offsets and total staging allocation size
  size_t total_staging_bytes = 0;
  std::vector<size_t> staging_offsets;
  staging_offsets.reserve(pending.size());

  uint32_t max_slot = allocated_textures.empty() ? 0 : static_cast<uint32_t>(allocated_textures.size() - 1);
  for (const auto& asset : pending) {
    staging_offsets.push_back(total_staging_bytes);
    total_staging_bytes += asset->pixel_data.size();
    max_slot = std::max(max_slot, bindless_texture_registry->get_index(asset));
  }
  allocated_textures.resize(max_slot + 1);

  // 2. Stage pixel data
  auto r_staging_buf = memory_allocator.create_buffer(vk::BufferUsageFlagBits::eTransferSrc, total_staging_bytes);
  if (!r_staging_buf) return mole::fail(r_staging_buf.error());
  vma::raii::Buffer staging_buffer = std::move(r_staging_buf).value();

  auto* staging_mapped = static_cast<uint8_t*>(staging_buffer.getAllocation().getInfo().pMappedData);
  for (size_t i = 0; i < pending.size(); ++i) {
    std::memcpy(staging_mapped + staging_offsets[i], pending[i]->pixel_data.data(), pending[i]->pixel_data.size());
  }

  // 3. Allocate and begin one-time command buffer
  auto r_cmd = allocate_upload_command_buffer();
  if (!r_cmd) return mole::fail(r_cmd.error());
  vk::raii::CommandBuffer cmd_buffer = std::move(r_cmd).value();

  // 4. Create GPU resources, prep barriers, and record transfers
  std::vector<vk::ImageMemoryBarrier2> pre_copy_barriers;
  std::vector<vk::ImageMemoryBarrier2> post_copy_barriers;
  std::vector<vk::DescriptorImageInfo> descriptor_image_infos;
  std::vector<vk::WriteDescriptorSet>  descriptor_writes;

  pre_copy_barriers.reserve(pending.size());
  post_copy_barriers.reserve(pending.size());
  descriptor_image_infos.reserve(pending.size());
  descriptor_writes.reserve(pending.size());

  for (size_t i = 0; i < pending.size(); ++i) {
    const auto& asset = pending[i];
    uint32_t slot = bindless_texture_registry->get_index(asset);

    auto r_gpu_image = create_texture_image(asset);
    if (!r_gpu_image) return mole::fail("Failed to create GPU texture image");
    vma::raii::Image gpu_image = std::move(r_gpu_image).value();

    auto r_gpu_view = create_texture_image_view(*gpu_image);
    if (!r_gpu_view) return mole::fail("Failed to create texture image view");
    vk::raii::ImageView gpu_view = std::move(r_gpu_view).value();

    // Inlined transition: Undefined -> TransferDstOptimal
    pre_copy_barriers.push_back(vk::ImageMemoryBarrier2{
        .srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask       = {},
        .dstStageMask        = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask       = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout           = vk::ImageLayout::eUndefined,
        .newLayout           = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = *gpu_image,
        .subresourceRange    = { .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 }
    });

    // Inlined transition: TransferDstOptimal -> ShaderReadOnlyOptimal
    post_copy_barriers.push_back(vk::ImageMemoryBarrier2{
        .srcStageMask        = vk::PipelineStageFlagBits2::eTransfer,
        .srcAccessMask       = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask       = vk::AccessFlagBits2::eShaderRead,
        .oldLayout           = vk::ImageLayout::eTransferDstOptimal,
        .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = *gpu_image,
        .subresourceRange    = { .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 }
    });

    // Staging to GPU copy command
    vk::BufferImageCopy copy_region{
        .bufferOffset      = staging_offsets[i],
        .bufferRowLength   = 0,
        .bufferImageHeight = 0,
        .imageSubresource  = { .aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
        .imageOffset       = { 0, 0, 0 },
        .imageExtent       = { static_cast<uint32_t>(asset->width), static_cast<uint32_t>(asset->height), 1 }
    };

    // Stash descriptor writes directly
    descriptor_image_infos.push_back({
                                         .sampler     = *default_sampler,
                                         .imageView   = *gpu_view,
                                         .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                                     });

    descriptor_writes.push_back({
                                    .dstSet          = *descriptor_set,
                                    .dstBinding      = 0,
                                    .dstArrayElement = slot,
                                    .descriptorCount = 1,
                                    .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                                    .pImageInfo      = &descriptor_image_infos.back()
                                });

    allocated_textures[slot] = GpuTexture{
        .image = std::move(gpu_image),
        .view  = std::move(gpu_view)
    };
  }

  // 5. Batch-record barriers and copies
  cmd_buffer.pipelineBarrier2(vk::DependencyInfo{
      .imageMemoryBarrierCount = static_cast<uint32_t>(pre_copy_barriers.size()),
      .pImageMemoryBarriers    = pre_copy_barriers.data()
  });

  for (size_t i = 0; i < pending.size(); ++i) {
    const auto& asset = pending[i];
    uint32_t slot = bindless_texture_registry->get_index(asset);

    vk::BufferImageCopy copy_region{
        .bufferOffset      = staging_offsets[i],
        .imageSubresource  = { .aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1 },
        .imageExtent       = { static_cast<uint32_t>(asset->width), static_cast<uint32_t>(asset->height), 1 }
    };
    cmd_buffer.copyBufferToImage(*staging_buffer, *allocated_textures[slot].image, vk::ImageLayout::eTransferDstOptimal, copy_region);
  }

  cmd_buffer.pipelineBarrier2(vk::DependencyInfo{
      .imageMemoryBarrierCount = static_cast<uint32_t>(post_copy_barriers.size()),
      .pImageMemoryBarriers    = post_copy_barriers.data()
  });

  // 6. Submit and wait
  if (cmd_buffer.end() != vk::Result::eSuccess) return mole::fail("Failed to end upload command buffer");

  vk::SubmitInfo submit_info{ .commandBufferCount = 1, .pCommandBuffers = &*cmd_buffer };
  vk_context.get_graphics_queue().submit(submit_info, nullptr);
  vk_context.get_graphics_queue().waitIdle();

  // 7. Write descriptors and finalize
  vk_context.get_logical_device().updateDescriptorSets(descriptor_writes, nullptr);
  bindless_texture_registry->clear_pending_texture_uploads();

  return mole::ok();
}

Result<vk::raii::CommandBuffer> VulkanMaterialHandler::allocate_upload_command_buffer() {
  vk::CommandBufferAllocateInfo alloc_info{
      .commandPool        = *command_pool,
      .level              = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1
  };
  auto vkr = vk_context.get_logical_device().allocateCommandBuffers(alloc_info);
  if (vkr.result != vk::Result::eSuccess) return mole::fail("Failed to allocate upload command buffer");

  vk::raii::CommandBuffer cmd = std::move(vkr.value[0]);
  if (cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}) != vk::Result::eSuccess) {
    return mole::fail("Failed to begin upload command buffer");
  }
  return cmd;
}

Result<vma::raii::Image> VulkanMaterialHandler::create_texture_image(const AssetHandle<ImageAsset>& asset) {
  vk::ImageCreateInfo image_info{
      .imageType     = vk::ImageType::e2D,
      .format        = vk::Format::eR8G8B8A8Srgb,
      .extent        = { static_cast<uint32_t>(asset->width), static_cast<uint32_t>(asset->height), 1 },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = vk::SampleCountFlagBits::e1,
      .tiling        = vk::ImageTiling::eOptimal,
      .usage         = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      .sharingMode   = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined
  };

  vma::AllocationCreateInfo alloc_info{
      .flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
      .usage = vma::MemoryUsage::eAuto
  };

  auto vkr = memory_allocator.get_allocator().createImage(image_info, alloc_info);
  if (vkr.result != vk::Result::eSuccess) return mole::fail("Failed to create GPU texture image");
  return std::move(vkr.value);
}

Result<vk::raii::ImageView> VulkanMaterialHandler::create_texture_image_view(vk::Image image) {
  vk::ImageViewCreateInfo view_info{
      .image            = image,
      .viewType         = vk::ImageViewType::e2D,
      .format           = vk::Format::eR8G8B8A8Srgb,
      .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 }
  };
  auto vkr = vk_context.get_logical_device().createImageView(view_info);
  if (vkr.result != vk::Result::eSuccess) return mole::fail("Failed to create texture image view");
  return std::move(vkr.value);
}

Result<void> VulkanMaterialHandler::prepare_materials(std::vector<MaterialID> material_ids) {
  if (material_ids.empty()) return mole::ok();

  std::vector<MaterialID> to_upload;
  to_upload.reserve(material_ids.size());
  std::vector<MaterialID> to_update;
  to_update.reserve(material_ids.size());

  for (MaterialID material_id : material_ids) {
    auto it = allocated_materials.find(material_id);
    if (it == allocated_materials.end()) {
      to_upload.push_back(material_id);
    } else {
      GpuMaterial& gpu_material = it->second;
      gpu_material.last_used_on_frame = frames_rendered.count;
      uint32_t version = material_pool->get_version(material_id);
      if (gpu_material.version != version) {
        to_update.push_back(material_id);
      }
    }
  }

  for (MaterialID mat_id : to_upload) {
    auto r_upload = upload_material(mat_id);
    if (!r_upload) return r_upload;
  }

  for (MaterialID mat_id : to_update) {
    auto r_update = update_material(mat_id);
    if (!r_update) return r_update;
  }

  auto r_update_texture_buffer = update_texture_buffer();
  if (!r_update_texture_buffer) return r_update_texture_buffer;

  return mole::ok();
}

Result<void> VulkanMaterialHandler::upload_material(const MaterialID& material_id) {
  std::byte* gpu_dest = static_cast<std::byte*>(material_buffer_mapped);

  // Increase texture references
  std::vector<uint64_t> texture_ids;
  const Material& material = material_pool->get(material_id);
  for (auto& [name, property] : material.properties) {
    if (auto texture = std::get_if<AssetHandle<ImageAsset>>(&property)) {
      uint64_t tex_id = texture->get_packed_id();
      add_texture_reference(tex_id);
      texture_ids.push_back(tex_id);
    }
  }

  auto r_packed = packed_material_cache->get(material_id);
  if (!r_packed) return mole::fail(r_packed.error());
  const PackedMaterial& packed_material = r_packed.value();

  if (packed_material.data.empty()) return mole::ok();

  if (packed_material.data.size() > MATERIAL_STRIDE) {
    return mole::fail("Packed material data is too large for VulkanMaterialHandler, it exceeds " + std::to_string(MATERIAL_STRIDE) + " bytes");
  }

  uint32_t slot_index = make_material_index();
  uint32_t shader_index = (slot_index * static_cast<uint32_t>(MATERIAL_STRIDE)) / static_cast<uint32_t>(packed_material.data.size());

  // Store slot, shader index, frame count, version, and texture references in the map!
  allocated_materials[material_id] = {
      .slot_index = slot_index,
      .shader_index = shader_index,
      .last_used_on_frame = frames_rendered.count,
      .version = material_pool->get_version(material_id),
      .texture_ids = std::move(texture_ids)
  };

  std::byte* target_address = gpu_dest + (slot_index * MATERIAL_STRIDE);
  std::memset(target_address, 0, MATERIAL_STRIDE);
  std::memcpy(target_address, packed_material.data.data(), packed_material.data.size());

  return mole::ok();
}

Result<void> VulkanMaterialHandler::update_material(const MaterialID& material_id) {
  std::byte* gpu_dest = static_cast<std::byte*>(material_buffer_mapped);

  auto it = allocated_materials.find(material_id);
  if (it == allocated_materials.end()) {
    return mole::fail("Material ID not found in allocated materials for update");
  }
  GpuMaterial& gpu_material = it->second;
  uint32_t slot_index = gpu_material.slot_index;

  std::vector<uint64_t> new_texture_ids;
  const Material& material = material_pool->get(material_id);
  for (auto& [name, property] : material.properties) {
    if (auto texture = std::get_if<AssetHandle<ImageAsset>>(&property)) {
      new_texture_ids.push_back(texture->get_packed_id());
    }
  }

  for (uint64_t tex_id : new_texture_ids) {
    add_texture_reference(tex_id);
  }

  for (uint64_t old_tex_id : gpu_material.texture_ids) {
    remove_texture_reference(old_tex_id);
  }

  gpu_material.texture_ids = std::move(new_texture_ids);

  auto r_packed = packed_material_cache->get(material_id);
  if (!r_packed) return mole::fail(r_packed.error());
  const PackedMaterial& packed_material = r_packed.value();

  if (packed_material.data.empty()) return mole::ok();

  if (packed_material.data.size() > MATERIAL_STRIDE) {
    return mole::fail("Packed material data is too large for VulkanMaterialHandler, it exceeds " + std::to_string(MATERIAL_STRIDE) + " bytes");
  }

  uint32_t shader_index = (slot_index * static_cast<uint32_t>(MATERIAL_STRIDE)) / static_cast<uint32_t>(packed_material.data.size());

  gpu_material.shader_index = shader_index;
  gpu_material.last_used_on_frame = frames_rendered.count;
  gpu_material.version = material_pool->get_version(material_id);

  std::byte* target_address = gpu_dest + (slot_index * MATERIAL_STRIDE);
  std::memset(target_address, 0, MATERIAL_STRIDE);
  std::memcpy(target_address, packed_material.data.data(), packed_material.data.size());

  return mole::ok();
}

void VulkanMaterialHandler::free_unused_materials() {
  // When erasing from a map while iterating, you must advance the iterator safely
  for (auto it = allocated_materials.begin(); it != allocated_materials.end(); ) {
    if ((frames_rendered.count - it->second.last_used_on_frame) > FREE_MATERIAL_AFTER_FRAMES) {
      MaterialID id_to_free = it->first;
      it++; // Advance BEFORE freeing so the iterator isn't invalidated
      free_material(id_to_free);
    } else {
      it++;
    }
  }
}

void VulkanMaterialHandler::free_material(const MaterialID& material_id) {
  auto it = allocated_materials.find(material_id);
  if (it != allocated_materials.end()) {
    uint32_t slot_index = it->second.slot_index;

    for (uint64_t texture_id : it->second.texture_ids) {
      remove_texture_reference(texture_id);
    }

    recycled_material_indices.push_back(slot_index);
    allocated_materials.erase(it);
  }
}

void VulkanMaterialHandler::add_texture_reference(uint64_t texture_id) {
  texture_ref_counts[texture_id]++;
}

void VulkanMaterialHandler::remove_texture_reference(uint64_t texture_id) {
  auto it = texture_ref_counts.find(texture_id);
  if (it != texture_ref_counts.end()) {
    if (it->second > 0) {
      it->second--;
    }
    // If count hits zero, the texture is completely unused
    if (it->second == 0) {
      free_texture(texture_id);
      texture_ref_counts.erase(it);
    }
  }
}

void VulkanMaterialHandler::free_texture(uint64_t texture_id) {
  auto r_index = bindless_texture_registry->get_existing_index(texture_id);

  if (!r_index) return;
  uint32_t index = r_index.value();
  allocated_textures[index] = GpuTexture{nullptr, nullptr};
  bindless_texture_registry->free_index(texture_id);
}

Result<uint32_t> VulkanMaterialHandler::get_material_index(const MaterialID& material_id) const {
  auto it = allocated_materials.find(material_id);
  if (it == allocated_materials.end()) {
    return mole::fail("Material ID not found in allocated materials");
  }

  return it->second.shader_index;
}

uint32_t VulkanMaterialHandler::make_material_index() {
  uint32_t index;
  if (!recycled_material_indices.empty()) {
    index = recycled_material_indices.back();
    recycled_material_indices.pop_back();
  } else {
    index = next_unused_material_index++;
  }

  return index;
}

} // mole