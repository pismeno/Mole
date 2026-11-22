#include <iostream>
#include <memory>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glslang/Public/ShaderLang.h>

#include "editor/editor.hpp"
#include "utils/result.hpp"

int main() {
  using namespace mole;

  glslang::InitializeProcess();

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW" << std::endl;
    return 1;
  }
  
  auto r_create_editor = Editor::create();
  if (!r_create_editor) {
    std::cerr << r_create_editor.error() << std::endl;
    return 1;
  }
  std::unique_ptr<Editor> editor = std::move(r_create_editor).value();

  auto r_run = editor->run();
  if (!r_run) {
    std::cerr << r_run.error() << std::endl;
    return 1;
  }

  glfwTerminate();
  glslang::FinalizeProcess();

  return 0;
}
