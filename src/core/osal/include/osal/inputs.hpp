#pragma once

#include <glm/vec2.hpp>

#include "osal/input_codes.hpp"

namespace mole::inputs {

/**
 * @brief Abstract static class for querying some IWindow about inputs.
 */
class Inputs {
 public:
  explicit Inputs() = default;
  virtual ~Inputs() noexcept = default;

  // Keyboard
  [[nodiscard]] virtual bool is_key_pressed(KeyCode key) noexcept = 0;    // True ONLY on the exact frame it was pressed
  [[nodiscard]] virtual bool is_key_held(KeyCode key) noexcept = 0;       // True as long as the key is down
  [[nodiscard]] virtual bool is_key_released(KeyCode key) noexcept = 0;   // True ONLY on the exact frame it was let go

  // Mouse
  [[nodiscard]] virtual bool is_mouse_button_pressed(MouseCode button) noexcept = 0;
  [[nodiscard]] virtual bool is_mouse_button_held(MouseCode button) noexcept = 0;
  [[nodiscard]] virtual bool is_mouse_button_released(MouseCode button) noexcept = 0;
  [[nodiscard]] virtual glm::vec2 get_mouse_position() noexcept = 0;
  [[nodiscard]] virtual float get_mouse_x() noexcept = 0;
  [[nodiscard]] virtual float get_mouse_y() noexcept = 0;
  [[nodiscard]] virtual glm::vec2 get_mouse_delta() noexcept = 0;
  [[nodiscard]] virtual float get_mouse_delta_x() noexcept = 0;
  [[nodiscard]] virtual float get_mouse_delta_y() noexcept = 0;
  [[nodiscard]] virtual float get_scroll_x() noexcept = 0;
  [[nodiscard]] virtual float get_scroll_y() noexcept = 0;
  [[nodiscard]] virtual float get_scroll_delta_x() noexcept = 0;
  [[nodiscard]] virtual float get_scroll_delta_y() noexcept = 0;

  /**
   * @brief Should be called after every frame.
   */
  virtual void end_frame() noexcept = 0;
};

} // mole::inputs
