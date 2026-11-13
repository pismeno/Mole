#pragma once

#include <glm/vec2.hpp>

#include "osal/input_codes.hpp"

namespace mole {

/**
 * @brief Pure abstract interface for querying some IWindow about inputs.
 */
class IInputs {
public:
  explicit IInputs() = default;
  virtual ~IInputs() noexcept = default;

  // Keyboard
  [[nodiscard]] virtual bool is_key_pressed(EKeyCode key) noexcept = 0;  // True ONLY on the exact frame it was pressed
  [[nodiscard]] virtual bool is_key_held(EKeyCode key) noexcept = 0;     // True as long as the key is down
  [[nodiscard]] virtual bool is_key_released(EKeyCode key) noexcept = 0; // True ONLY on the exact frame it was let go

  // Mouse
  [[nodiscard]] virtual bool is_mouse_button_pressed(EMouseCode button) noexcept = 0;
  [[nodiscard]] virtual bool is_mouse_button_held(EMouseCode button) noexcept = 0;
  [[nodiscard]] virtual bool is_mouse_button_released(EMouseCode button) noexcept = 0;
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

} // mole