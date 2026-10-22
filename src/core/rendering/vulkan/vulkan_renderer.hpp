#pragma once

#include "rendering/irenderer.hpp"

#ifdef BIRD_PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/vulkan/quad.hpp"
#include "rendering/vulkan/render_batch.hpp"
#include "resources/material_types.hpp"
#include "resources/render_commands.hpp"
#include "resources/view_data.hpp"
#include "utils/counter.hpp"
#include "utils/result.hpp"

namespace mole {

class AssetManager;
class MaterialPool;
class VulkanContext;
class VulkanMaterialHandler;
class VulkanMemoryAllocator;
class VulkanPipelineCache;
class VulkanSwapchain;
struct RenderingContext;

class VulkanRenderer : public IRenderer {
 public:
  ~VulkanRenderer();

  static constexpr inline uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  static constexpr inline size_t MAX_QUADS = 100000;

  void begin_frame() override;
  void begin_pass(const ViewData& view_data) override;
  void submit(const RenderQuadCommand render_command) override;
  void submit(const std::vector<RenderQuadCommand>& render_commands) override;
  void end_pass() override;
  void end_frame() override;

  Result<void> resize_framebuffer(uint32_t width, uint32_t height) override;
  bool needs_framebuffer_resize() const noexcept override;
  bool is_context_lost() const noexcept override;

  static Result<std::unique_ptr<VulkanRenderer>> create(RenderingContext& context, void* native_window_handle, uint32_t window_width, uint32_t window_height);
 private:
  explicit VulkanRenderer() = default;

  struct FrameData {
    vk::raii::Semaphore image_available_semaphore = nullptr;
    vk::raii::Fence in_flight_fence = nullptr;

    vk::raii::CommandBuffer command_buffer = nullptr;

    vma::raii::Buffer uniform_buffer = nullptr;
    void* uniform_buffer_mapped = nullptr;
    vma::raii::Buffer quad_buffer = nullptr;
    Quad* quad_buffer_mapped = nullptr;

    vk::raii::DescriptorSet descriptor_set = nullptr;
  };

  Result<void> create_command_pool();
  Result<void> create_descriptor_set_layout();
  Result<void> create_frame_data();

  void build_batches();
  void update_ubo(const ViewData& view_data, FrameData& frame);
  void update_quad_ssbo(FrameData& frame);
  inline void move_to_next_frame();

  void transition_image_layout(
      vk::raii::CommandBuffer& cmd_buffer,
      vk::Image                image,
      vk::ImageLayout         old_layout,
      vk::ImageLayout         new_layout,
      vk::AccessFlags2        src_access_mask,
      vk::AccessFlags2        dst_access_mask,
      vk::PipelineStageFlags2 src_stage_mask,
      vk::PipelineStageFlags2 dst_stage_mask);

  std::unique_ptr<Counter> frames_rendered;
  std::unique_ptr<VulkanContext> vk_context;
  std::unique_ptr<VulkanMemoryAllocator> memory_allocator;
  std::unique_ptr<VulkanSwapchain> swapchain;
  std::unique_ptr<VulkanMaterialHandler> material_handler;
  std::unique_ptr<VulkanPipelineCache> pipeline_cache;

  vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
  vk::raii::DescriptorPool ubo_descriptor_pool = nullptr;

  vk::raii::CommandPool command_pool = nullptr;

  std::vector<FrameData> frames;
  std::vector<vk::raii::Semaphore> render_finished_semaphores;

  // rendering states
  bool is_context_lost_ = false;
  bool needs_framebuffer_resize_ = false;
  uint32_t current_image_index_ = 0;
  uint32_t current_frame_ = 0;
  ViewData current_view_data_;

  // mole systems
  AssetManager* asset_manager = nullptr;
  MaterialPool* material_pool = nullptr;

  std::vector<RenderQuadCommand> pending_commands;
  std::vector<RenderBatch> batches;
  std::vector<Quad> quads;
  std::vector<MaterialID> batch_materials_pool;

  std::vector<MaterialID> get_needed_pass_materials();
};

} // mole
