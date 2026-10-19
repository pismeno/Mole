#pragma once

#include <osal/iwindow.hpp>

#include "GLFW/glfw3.h"

#include "osal/inputs/glfw/glfw_inputs.hpp"
#include <utils/result.hpp>

namespace mole {

using namespace inputs;

class GlfwWindow : public IWindow {

  friend class inputs::GlfwInputs;
  friend class IWindow;

 public:
  virtual ~GlfwWindow();

  Result<void> init() override;
  void update() override;
  Result<void> close() override;
  bool should_close() const noexcept override;
  uint32_t get_logical_width() const override;
  uint32_t get_logical_height() const override;
  uint32_t get_framebuffer_width() const override;
  uint32_t get_framebuffer_height() const override;
  bool is_focused() const noexcept override;
  bool is_fullscreen() const noexcept override;
  bool is_minimized() const noexcept override;
  void set_fullscreen(bool fullscreen) override;
  Inputs& get_inputs() override;
  void* get_native_handle() const override;

 private:
  GlfwWindow(const WindowOptions& options);

  GLFWwindow* glfw_window{};

  std::string title{};

  uint32_t logical_width{};
  uint32_t logical_height{};
  uint32_t framebuffer_width{};
  uint32_t framebuffer_height{};

  uint32_t windowed_width{};
  uint32_t windowed_height{};
  int windowed_x{}, windowed_y{};
  bool is_fullscreen_{};

  GlfwInputs inputs;

  void window_resize_callback(GLFWwindow* window, int width, int height);
  void frame_buffer_resize_callback(GLFWwindow* window, int width, int height);
};

} // mole