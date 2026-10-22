#include "osal/inputs/glfw/glfw_inputs.hpp"

#include <cstdint>

#include <GLFW/glfw3.h>

#include "osal/windowing/glfw/glfw_window.hpp"

namespace mole::inputs {

bool GlfwInputs::is_key_held(KeyCode key) noexcept {
  return currentKeyState[(uint16_t) key];
}

bool GlfwInputs::is_key_pressed(KeyCode key) noexcept {
  return currentKeyState[(uint16_t) key] && !previousKeyState[(uint16_t) key];
}

bool GlfwInputs::is_key_released(KeyCode key) noexcept {
  return !currentKeyState[(uint16_t) key] && previousKeyState[(uint16_t) key];
}

bool GlfwInputs::is_mouse_button_held(MouseCode button) noexcept {
  return currentMouseState[(uint16_t)button];
}

bool GlfwInputs::is_mouse_button_pressed(MouseCode button) noexcept {
  return currentMouseState[(uint16_t)button] && !previousMouseState[(uint16_t)button];
}

bool GlfwInputs::is_mouse_button_released(MouseCode button) noexcept {
  return !currentMouseState[(uint16_t)button] && previousMouseState[(uint16_t)button];
}

glm::vec2 GlfwInputs::get_mouse_position() noexcept {
  return glm::vec2(mouseX, mouseY);
}

float GlfwInputs::get_mouse_x() noexcept {
  return mouseX;
}

float GlfwInputs::get_mouse_y() noexcept {
  return mouseY;
}

glm::vec2 GlfwInputs::get_mouse_delta() noexcept {
  return glm::vec2(get_mouse_delta_x(), get_mouse_delta_y());
}

float GlfwInputs::get_mouse_delta_y() noexcept {
  return mouseY - previousMouseY;
}

float GlfwInputs::get_mouse_delta_x() noexcept {
  return mouseX - previousMouseX;
}

float GlfwInputs::get_scroll_x() noexcept {
  return scrollX;
}

float GlfwInputs::get_scroll_y() noexcept {
  return scrollY;
}

float GlfwInputs::get_scroll_delta_x() noexcept {
  return scrollX - previousScrollX;
}

float GlfwInputs::get_scroll_delta_y() noexcept {
  return scrollY - previousScrollY;
}

void GlfwInputs::end_frame() noexcept {
  endFrameKeys();
  endFrameMouse();
}

inline void GlfwInputs::endFrameKeys() noexcept {
  for (int i = 0; i < MAX_KEYS; i++) {
    previousKeyState[i] = currentKeyState[i];
  }
}

inline void GlfwInputs::endFrameMouse() noexcept {
  for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) {
    previousMouseState[i] = currentMouseState[i];
  }
}

void GlfwInputs::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto* mole_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));

  if (mole_window) {
    mole_window->inputs.handleKey(key, scancode, action, mods);
  }
}

void GlfwInputs::mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
  auto* mole_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));

  if (mole_window) {
    mole_window->inputs.handleMouseButton(button, action, mods);
  }
}

void GlfwInputs::cursor_pos_callback(GLFWwindow *window, double xpos, double ypos) {
  auto* mole_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));

  if (mole_window) {
    mole_window->inputs.handle_cursor(xpos, ypos);
  }
}

void GlfwInputs::scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  auto* mole_window = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));

  if (mole_window) {
    mole_window->inputs.handleScroll(xoffset, yoffset);
  }
}

void GlfwInputs::handleKey(int key, int scancode, int action, int mods) noexcept {
  if (key < 0 || key >= MAX_KEYS) {
    return; // Ignore out-of-bounds keys
  }

  // Converting to local key codes is not needed, since GLFW codes are matching with ours
  if (action == GLFW_PRESS) {
    currentKeyState[key] = true;
  } else if (action == GLFW_RELEASE) {
    currentKeyState[key] = false;
  }
}

void GlfwInputs::handleMouseButton(int button, int action, int mods) noexcept {
  if (button < 0 || action >= MAX_MOUSE_BUTTONS) {
    return; // Ignore out-of-bounds keys
  }

  // Converting to local key codes is not needed, since GLFW codes are matching with ours
  if (action == GLFW_PRESS) {
    currentMouseState[button] = true;
  } else if (action == GLFW_RELEASE) {
    currentMouseState[button] = false;
  }
}

void GlfwInputs::handle_cursor(double xpos, double ypos) noexcept {
  auto x = (float)xpos;
  auto y = (float)ypos;

  mouseX = x;
  mouseY = y;
}

void GlfwInputs::handleScroll(double xoffset, double yoffset) noexcept {
  auto x = (float)xoffset;
  auto y = (float)yoffset;

  scrollX = x;
  scrollY = y;
}

} // mole::inputs
