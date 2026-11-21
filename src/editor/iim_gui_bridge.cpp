#include "editor/iim_gui_bridge.hpp"

#if defined(MOLE_PLATFORM_WINDOWS)
#include "im_gui_bridge/win32_vulkan_im_gui_bridge.hpp"
#endif
#include "utils/result.hpp"

namespace mole {

Result<std::unique_ptr<IImGuiBridge>> IImGuiBridge::create(const ImGuiBridgeOptions& options,
                                                           const EditorContext& context) {
  switch (options.renderer_type) {
    case ERendererType::Vulkan: {
#if defined(MOLE_PLATFORM_WINDOWS)
      auto r_bridge = Win32VulkanImGuiBridge::create(context, options.native_window_handle);
      if (!r_bridge) return mole::fail(r_bridge.error());
      return std::unique_ptr<IImGuiBridge>(std::move(r_bridge).value());
#else
      return mole::fail("Vulkan ImGui bridge is only supported on Windows");
#endif
    }
    default:
      return mole::fail("Unsupported renderer type for ImGui bridge");
  }
}

} // namespace mole
