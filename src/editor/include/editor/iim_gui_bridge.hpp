#pragma once

#include <cstdint>
#include <memory>

#include "rendering/renderer_types.hpp"
#include "utils/result.hpp"

namespace mole {

struct ImGuiBridgeOptions {
  ERendererType renderer_type;
  void* native_window_handle;
  PresenterID presenter; // ImGui renders into this presenter; its pipeline is built for the presenter's format
};

struct EditorContext;

class IImGuiBridge {
public:
  virtual ~IImGuiBridge() = default;

  virtual void begin_frame() = 0;
  virtual Result<void> end_frame() = 0;

  /** @brief ImTextureID for sampling an offscreen render target with ImGui::Image. Re-query every frame (resize swaps the view). The target must have been rendered at least once. */
  [[nodiscard]] virtual Result<uint64_t> get_texture_id(RenderTargetID target) = 0;
  /** @brief Releases the texture for `target` (deferred until frames in flight finish). Call before destroying the target. */
  virtual void release_texture(RenderTargetID target) = 0;

  static Result<std::unique_ptr<IImGuiBridge>> create(const ImGuiBridgeOptions& options, const EditorContext& context);
};

} // mole
