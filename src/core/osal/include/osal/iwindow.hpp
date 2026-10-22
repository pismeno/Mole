#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "utils/result.hpp"

namespace mole {

namespace inputs {
class Inputs;
}

struct WindowOptions {
  uint32_t logical_width, logical_height;

  std::string title;

  bool start_fullscreen = false;
};

class IWindow {
 public:
  explicit IWindow() = default;
  virtual ~IWindow() noexcept = default;

  /**
   * @brief Updates the window, should be called every frame.
   */
  virtual void update() = 0;

  /**
   * @brief Closes the window.
   */
  virtual Result<void> close() = 0;

  /**
   * @brief Returns true if the window should be closed (for example when the windows X button is pressed).
   * Should be checked every frame.
   */
  [[nodiscard]] virtual bool should_close() const noexcept = 0;

  [[nodiscard]] virtual uint32_t get_logical_width() const = 0;
  [[nodiscard]] virtual uint32_t get_logical_height() const = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_width() const = 0;
  [[nodiscard]] virtual uint32_t get_framebuffer_height() const = 0;
  [[nodiscard]] virtual bool is_focused() const noexcept = 0;
  [[nodiscard]] virtual bool is_fullscreen() const noexcept = 0;
  [[nodiscard]] virtual bool is_minimized() const noexcept = 0;
  virtual void set_fullscreen(bool fullscreen) = 0;

  /**
   * @return Inputs object, responsible for handling input events.
   */
  virtual inputs::Inputs& get_inputs() = 0;

  /**
   * @return OS specific handle to the window.
   */
  [[nodiscard]] virtual void* get_native_handle() const = 0;

  /**
   * @brief Factory method to create the specific implementation.
   */
  static Result<std::unique_ptr<IWindow>> create(const WindowOptions& options);
 private:
  virtual Result<void> init() = 0;
};

} // mole
