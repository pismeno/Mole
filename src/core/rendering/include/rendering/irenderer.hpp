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
class IWindow;

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

  virtual Result<RenderTargetID> create_render_target(const uint32_t width, uint32_t height);
  virtual Result<PresenterID> create_presenter(const IWindow& window);

  virtual Result<void> destroy(RenderTargetID render_target_id);
  virtual Result<void> destroy(PresenterID presenter_id);

  virtual Result<void> resize_framebuffer(PresenterID presenter_id, uint32_t width, uint32_t height) = 0;
  virtual bool needs_framebuffer_resize(PresenterID presenter_id) const noexcept = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_width(RenderTargetID render_target_id) const noexcept = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_height(RenderTargetID render_target_id) const noexcept = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_width(PresenterID presenter_id) const noexcept = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_height(PresenterID presenter_id) const noexcept = 0;
  
  virtual bool is_context_lost() const noexcept = 0;

  virtual void* get_native_handle(ERendererNativeHandle handle_type) const = 0;
  virtual void* get_native_handle(RenderTargetID render_target_id) const = 0;
  virtual void* get_native_handle(PresenterID presenter_id) const = 0;
  virtual RendererBackendInfo get_backend_info() const = 0;

  template <typename TCast, typename TArg>
  TCast get_native_handle(TArg arg) const { return reinterpret_cast<TCast>(get_native_handle(arg)); }

  /**
   * @brief Factory method to create the specific implementation
   */
  static Result<std::unique_ptr<IRenderer>> create(ERendererType type, RenderingContext& context);
};

} // mole
