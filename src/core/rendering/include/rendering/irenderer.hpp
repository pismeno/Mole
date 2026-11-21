#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "rendering/renderer_types.hpp"
#include "resources/render_commands.hpp"
#include "resources/view_data.hpp"
#include "utils/result.hpp"

namespace mole {

struct RenderingContext;

struct RendererOptions {
  ERendererType type;
  void* native_window_handle;
  uint32_t window_width, window_height;
};

class IRenderer {
public:
  explicit IRenderer() = default;
  virtual ~IRenderer() = default;

  virtual void begin_frame() = 0;
  virtual void begin_pass(const ViewData& view_data) = 0;
  virtual void submit(const RenderQuadCommand render_command) = 0;
  virtual void submit(const std::vector<RenderQuadCommand>& render_commands) = 0;
  virtual void end_pass() = 0;
  virtual void end_frame() = 0;

  virtual Result<void> resize_framebuffer(uint32_t width, uint32_t height) = 0;
  virtual bool needs_framebuffer_resize() const noexcept = 0;
  virtual bool is_context_lost() const noexcept = 0;

  [[nodiscard]] virtual uint32_t get_framebuffer_width() const noexcept = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_height() const noexcept = 0;

  virtual void* get_native_handle(ERendererNativeHandle handle_type) const = 0;
  virtual RendererBackendInfo get_backend_info() const = 0;

  template <typename T>
  T get_native_handle(ERendererNativeHandle handle_type) const { return reinterpret_cast<T>(get_native_handle(handle_type)); }

  /**
   * @brief Factory method to create the specific implementation
   */
  static Result<std::unique_ptr<IRenderer>> create(const RendererOptions& options, RenderingContext& context);
};

} // mole
