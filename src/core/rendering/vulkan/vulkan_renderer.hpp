#pragma once

#include "rendering/irenderer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <vk_mem_alloc_raii.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "rendering/renderer_types.hpp"
#include "rendering/vulkan/quad.hpp"
#include "rendering/vulkan/render_batch.hpp"
#include "rendering/vulkan/vulkan_context.hpp"
#include "resources/material_types.hpp"
#include "resources/render_commands.hpp"
#include "resources/view_data.hpp"
#include "utils/counter.hpp"
#include "utils/result.hpp"
#include "utils/slot_map.hpp"

namespace mole {

class AssetManager;
class IWindow;
class MaterialPool;
class VulkanMaterialHandler;
class VulkanMemoryAllocator;
class VulkanPipelineCache;
class VulkanPresenter;
class VulkanRenderTarget;
struct RenderingContext;

class VulkanRenderer : public IRenderer {
public:
  ~VulkanRenderer() override;

  static constexpr inline uint32_t MAX_FRAMES_IN_FLIGHT = VulkanContext::MAX_FRAMES_IN_FLIGHT;
  static constexpr inline size_t MAX_QUADS = 100000;
  static constexpr inline uint32_t MAX_PASSES_PER_FRAME = 16;

  Result<void> begin_frame() override;
  Result<bool> begin_pass(RenderTargetID target, const PassOptions& options) override;
  void submit(const RenderQuadCommand& render_command) override;
  void submit(const std::vector<RenderQuadCommand>& render_commands) override;
  Result<void> end_pass() override;
  Result<void> end_frame() override;

  Result<RenderTargetID> create_render_target(const RenderTargetOptions& options) override;
  Result<PresenterID> create_presenter(const IWindow* window) override;
  Result<void> resize(RenderTargetID target, uint32_t width, uint32_t height) override;
  Result<void> destroy(RenderTargetID target) override;
  Result<void> destroy(PresenterID presenter) override;

  [[nodiscard]] RenderTargetID get_render_target(PresenterID presenter) const override;
  [[nodiscard]] Result<RenderTargetInfo> get_info(RenderTargetID target) const override;
  [[nodiscard]] Result<PresenterInfo> get_info(PresenterID presenter) const override;

  [[nodiscard]] void* get_native_handle(ERendererNativeHandle type) const override;
  [[nodiscard]] void* get_native_handle(RenderTargetID target, ERenderTargetNativeHandle type) const override;
  [[nodiscard]] void* get_native_handle(PresenterID presenter) const override;
  [[nodiscard]] RendererBackendInfo get_backend_info() const override;

  static Result<std::unique_ptr<VulkanRenderer>> create(RenderingContext& context);

private:
  explicit VulkanRenderer(const RenderingContext& context);

  Result<void> init(RenderingContext& context);

  struct FrameData {
    vk::raii::Fence in_flight_fence = nullptr;

    vk::raii::CommandBuffer command_buffer = nullptr;

    vma::raii::Buffer uniform_buffer = nullptr;
    std::byte* uniform_buffer_mapped = nullptr;
    vma::raii::Buffer quad_buffer = nullptr;
    Quad* quad_buffer_mapped = nullptr;

    vk::raii::DescriptorSet descriptor_set = nullptr;
  };

  /// An offscreen RT owns its image; a presenter RT refers back to the presenter's backbuffer.
  using RenderTargetEntry = std::variant<std::unique_ptr<VulkanRenderTarget>, PresenterID>;

  struct PresenterEntry {
    std::unique_ptr<VulkanPresenter> presenter;
    RenderTargetID render_target;
  };

  struct RetiredRenderTarget {
    uint64_t retire_after_frame;
    std::unique_ptr<VulkanRenderTarget> render_target;
  };

  struct ActivePass {
    RenderTargetID target;
    vk::Image image;
    vk::Format format;
    vk::ImageLayout* layout;
    bool is_offscreen;
    ViewData view_data;
  };

  Result<void> create_command_pool();
  Result<void> create_descriptor_set_layout();
  Result<void> create_frame_data();

  void build_batches();
  void record_draws(FrameData& frame, const ActivePass& pass);
  void retire(std::unique_ptr<VulkanRenderTarget> render_target);
  inline void move_to_next_frame();

  [[nodiscard]] std::vector<MaterialID> get_needed_pass_materials();

  void transition_image_layout(vk::raii::CommandBuffer& cmd_buffer, vk::Image image, vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
                               vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask,
                               vk::PipelineStageFlags2 dst_stage_mask);

  // mole systems
  AssetManager* asset_manager;
  MaterialPool* material_pool;

  std::unique_ptr<Counter> frames_rendered;
  std::unique_ptr<VulkanContext> vk_context;
  std::unique_ptr<VulkanMemoryAllocator> memory_allocator;
  std::unique_ptr<VulkanMaterialHandler> material_handler;
  std::unique_ptr<VulkanPipelineCache> pipeline_cache;

  vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
  vk::raii::DescriptorPool ubo_descriptor_pool = nullptr;

  vk::raii::CommandPool command_pool = nullptr;

  std::vector<FrameData> frames;

  // Declared after the context and allocator so they're destroyed first.
  SlotMap<RenderTargetID, RenderTargetEntry> render_targets;
  SlotMap<PresenterID, PresenterEntry> presenters;
  std::vector<RetiredRenderTarget> retired_render_targets;

  // rendering states
  uint32_t current_frame = 0;
  size_t ubo_stride = 0;
  bool in_frame = false;
  std::optional<ActivePass> active_pass;
  std::vector<PresenterID> acquired_presenters;
  uint32_t pass_cursor = 0;
  size_t quad_cursor = 0;

  std::vector<RenderQuadCommand> pending_commands;
  std::vector<RenderBatch> batches;
  std::vector<Quad> quads;
  std::vector<MaterialID> batch_materials_pool;
};

} // mole
