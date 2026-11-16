#pragma once

#include "rendering/irenderer.hpp"
#include <QuartzCore/CAMetalLayer.hpp>

#include <filesystem>
#include <stb/stb_image.h>

#include "rendering/vertex.hpp"
#include "rendering/texture.hpp"
#include "node_system/render_gatherer.hpp"
#include "utils/result.hpp"

namespace MTL {
class RenderCommandEncoder;
}

struct GLFWwindow;

namespace mole {


class MetalRenderer : public IRenderer {
public:
  Result<void> attach_window(void* native_window_handle, uint32_t window_width, uint32_t window_height) override;
  Result<void> init() override;
  void render() override;
  Result<void> shutdown() override;
  Result<void> resize_frame_buffer(int width, int height);
  void submit_quad(const RenderCommand render_command) override;

  Result<void> resize_framebuffer(uint32_t width, uint32_t height) override;

  [[nodiscard]] uint32_t get_framebuffer_width() const noexcept override { return 0; }
  [[nodiscard]] uint32_t get_framebuffer_height() const noexcept override { return 0; }

private:
  void init_device();

  void create_default_library();
  void create_command_queue();
  void create_render_pipeline();
  void create_texture();

  void encode_render_command(MTL::RenderCommandEncoder* render_command_encoder);
  void send_render_command();
  void draw();

  struct Impl;
  Impl* metal_backend_ = nullptr;

  Texture2D* texture = nullptr;
};

}