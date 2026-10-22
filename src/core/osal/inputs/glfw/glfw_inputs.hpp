#pragma once

#include "osal/inputs.hpp"

struct GLFWwindow;

namespace mole {
class GlfwWindow;
}

namespace mole::inputs {

/**
 * @brief Class for handling GLFW inputs
 */
class GlfwInputs : public Inputs {

  friend class ::mole::GlfwWindow;

 public:
  explicit GlfwInputs() = default;

  // Keyboard
  bool is_key_pressed(KeyCode key) noexcept override;    // True ONLY on the exact frame it was pressed
  bool is_key_held(KeyCode key) noexcept override;       // True as long as the key is down
  bool is_key_released(KeyCode key) noexcept override;   // True ONLY on the exact frame it was let go

  // Mouse
  bool is_mouse_button_pressed(MouseCode button) noexcept override;
  bool is_mouse_button_held(MouseCode button) noexcept override;
  bool is_mouse_button_released(MouseCode button) noexcept override;
  glm::vec2 get_mouse_position() noexcept override;
  float get_mouse_x() noexcept override;
  float get_mouse_y() noexcept override;
  glm::vec2 get_mouse_delta() noexcept override;
  float get_mouse_delta_x() noexcept override;
  float get_mouse_delta_y() noexcept override;
  float get_scroll_x() noexcept override;
  float get_scroll_y() noexcept override;
  float get_scroll_delta_x() noexcept override;
  float get_scroll_delta_y() noexcept override;

  void end_frame() noexcept override;

 private:
  static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
  static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
  static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
  void handleKey(int key, int scancode, int action, int mods) noexcept;
  void handleMouseButton(int button, int action, int mods) noexcept;
  void handle_cursor(double xpos, double ypos) noexcept;
  void handleScroll(double xoffset, double yoffset) noexcept;

  // Mouse
  static const int MAX_MOUSE_BUTTONS = 16;
  bool currentMouseState[MAX_MOUSE_BUTTONS]{};
  bool previousMouseState[MAX_MOUSE_BUTTONS]{};

  float mouseX = 0.0f;
  float mouseY = 0.0f;
  float previousMouseX = 0.0f;
  float previousMouseY = 0.0f;

  float scrollX = 0.0f;
  float scrollY = 0.0f;
  float previousScrollX = 0.0f;
  float previousScrollY = 0.0f;

  // Keyboard
  static const int MAX_KEYS = 512;
  bool currentKeyState[MAX_KEYS]{};
  bool previousKeyState[MAX_KEYS]{};

  inline void endFrameKeys() noexcept;
  inline void endFrameMouse() noexcept;
};

} // mole::inputs
