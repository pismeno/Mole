#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "rendering/renderer_types.hpp"
#include "resources/render_commands.hpp"
#include "utils/result.hpp"

namespace mole {

struct RenderingContext;
class IWindow;

/**
 * @brief Backend-agnostic renderer.
 *
 * Concepts and their backend mapping:
 * - Render target (RT): VkImage / ID3D12Resource / MTLTexture.
 * - Presenter: VkSwapchainKHR / IDXGISwapChain3 / CAMetalLayer. Exposes its backbuffer as a stable RenderTargetID.
 * - Pass: vkCmdBeginRendering / OMSetRenderTargets / MTLRenderCommandEncoder.
 * - Present (implicit, in end_frame): vkQueuePresentKHR / Present / presentDrawable:.
 *
 * Threading: the whole API is main-thread-only.
 *
 * External renderers (e.g. ImGui) record native commands inside an open pass via
 * ERendererNativeHandle::ActiveCommandBuffer. Those commands run *before* the pass's submitted quads, because quads
 * are batched and flushed in end_pass.
 */
class IRenderer {
public:
  explicit IRenderer() = default;
  virtual ~IRenderer() = default;

  /**
   * @brief Starts a frame. Fails only on fatal errors. Every successful call must be matched by end_frame().
   */
  virtual Result<void> begin_frame() = 0;

  /**
   * @brief Opens a pass on `target`, clearing it to `options.clear_color`, or loading its contents if nullopt.
   *
   * For a presenter's RT, the first pass in a frame acquires the backbuffer, recreating the swapchain if needed.
   * - ok(true): the pass is open. Call submit() and then end_pass().
   * - ok(false): transient, the target can't be rendered this frame (minimized / zero extent, or the swapchain is
   *   still out of date after one recreate-and-retry). The pass is not open; skip it. Not an error.
   * - fail: fatal, or a programmer error (stale/invalid ID, a pass already open, called outside a frame). Programmer
   *   errors also assert in debug.
   * Only one pass can be open at a time.
   */
  virtual Result<bool> begin_pass(RenderTargetID target, const PassOptions& options) = 0;

  /** @brief Queues a quad for the open pass. Only appends to a CPU list. */
  virtual void submit(const RenderQuadCommand& render_command) = 0;

  /** @brief Queues quads for the open pass. Only appends to a CPU list. */
  virtual void submit(const std::vector<RenderQuadCommand>& render_commands) = 0;

  /**
   * @brief Flushes the queued quads and closes the pass. Afterwards an offscreen RT can be sampled.
   * Fails only on fatal errors (e.g. device lost during the texture uploads it triggers).
   */
  virtual Result<void> end_pass() = 0;

  /**
   * @brief Submits the frame's GPU work and presents every presenter that was drawn into this frame.
   *
   * Presenters that weren't drawn into are untouched. An out-of-date/suboptimal present is not an error: the
   * presenter recreates itself on its next acquire. Fails only on fatal errors.
   */
  virtual Result<void> end_frame() = 0;

  /** @brief Creates an offscreen render target. */
  virtual Result<RenderTargetID> create_render_target(const RenderTargetOptions& options) = 0;

  /**
   * @brief Creates a presenter (swapchain) for `window`. Resizes are handled automatically.
   * The window must outlive the presenter: destroy the presenter first.
   */
  virtual Result<PresenterID> create_presenter(const IWindow* window) = 0;

  /**
   * @brief Resizes an offscreen render target. Fails for presenter-owned RTs.
   * Old GPU resources are released once in-flight frames finish; native handles from before the resize become invalid.
   */
  virtual Result<void> resize(RenderTargetID target, uint32_t width, uint32_t height) = 0;

  /**
   * @brief Destroys an offscreen render target. Fails for presenter-owned RTs.
   * GPU resources are released once in-flight frames finish.
   */
  virtual Result<void> destroy(RenderTargetID target) = 0;

  /** @brief Destroys a presenter and its backbuffer RT. GPU resources are released once in-flight frames finish. */
  virtual Result<void> destroy(PresenterID presenter) = 0;

  /**
   * @brief Returns the presenter's backbuffer RT. The ID is stable for the presenter's lifetime. Its native handles are
   * only valid during a pass, or after begin_pass acquired the backbuffer this frame; otherwise they are nullptr.
   */
  [[nodiscard]] virtual RenderTargetID get_render_target(PresenterID presenter) const = 0;

  /** @brief Returns the current size and format of a render target. */
  [[nodiscard]] virtual Result<RenderTargetInfo> get_info(RenderTargetID target) const = 0;

  /** @brief Returns the current size, image count and format of a presenter's swapchain. */
  [[nodiscard]] virtual Result<PresenterInfo> get_info(PresenterID presenter) const = 0;

  /** @brief Returns a backend-wide native handle, or nullptr if unavailable. */
  [[nodiscard]] virtual void* get_native_handle(ERendererNativeHandle type) const = 0;

  /** @brief Returns a native handle of a render target, or nullptr if unavailable. */
  [[nodiscard]] virtual void* get_native_handle(RenderTargetID target, ERenderTargetNativeHandle type) const = 0;

  /** @brief Returns the presenter's native swapchain object, or nullptr if unavailable. */
  [[nodiscard]] virtual void* get_native_handle(PresenterID presenter) const = 0;

  /** @brief Returns backend-wide information (queue families, frames in flight). */
  [[nodiscard]] virtual RendererBackendInfo get_backend_info() const = 0;

  /** @brief Typed convenience wrapper around the get_native_handle overloads. */
  template <typename TCast, typename... TArgs>
  [[nodiscard]] TCast get_native_handle(TArgs... args) const {
    return reinterpret_cast<TCast>(get_native_handle(args...));
  }

  /**
   * @brief Factory method to create the specific implementation
   */
  static Result<std::unique_ptr<IRenderer>> create(ERendererType type, RenderingContext& context);
};

} // mole
