#pragma once

#include <memory>

#include "editor/iim_gui_bridge.hpp"
#include "utils/result.hpp"

namespace mole {

class Win32VulkanImGuiBridge : public IImGuiBridge {
public:
  void begin_frame() override;
  void end_frame() override;

  static Result<std::unique_ptr<Win32VulkanImGuiBridge>> create();
};
  
} // mole