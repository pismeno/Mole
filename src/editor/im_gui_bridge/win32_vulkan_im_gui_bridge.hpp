#pragma once

#include <memory>

#include "editor/iim_gui_bridge.hpp"
#include "utils/result.hpp"

namespace mole {

struct EditorContext;
class IRenderer;

class Win32VulkanImGuiBridge : public IImGuiBridge {
public:
  ~Win32VulkanImGuiBridge() override;

  void begin_frame() override;
  void end_frame() override;

  static Result<std::unique_ptr<Win32VulkanImGuiBridge>> create(const EditorContext& context, void* window_handle);
private:
  explicit Win32VulkanImGuiBridge(const EditorContext& context, void* window_handle);

  Result<void> init();

  IRenderer* renderer;
  void* window_handle;
};

} // mole