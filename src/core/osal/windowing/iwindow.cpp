#include "osal/iwindow.hpp"

#include <memory>
#include <utility>

#include "osal/windowing/glfw/glfw_window.hpp"
#include "utils/result.hpp"

namespace mole {

Result<std::unique_ptr<IWindow>> IWindow::create(const WindowOptions& options) {
  auto window = std::unique_ptr<GlfwWindow>(new GlfwWindow(options));
  auto r_init = window->init();
  if (!r_init) return mole::fail(r_init.error());
  return std::move(window);
}

} // mole
