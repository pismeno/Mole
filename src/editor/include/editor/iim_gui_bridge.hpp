#pragma once

#include <memory>

#include "rendering/renderer_types.hpp"
#include "utils/result.hpp"

namespace mole {

struct ImGuiBridgeOptions {
  ERendererType renderer_type;
  void* native_window_handle;
};

struct EditorContext;

class IImGuiBridge {
public:
  virtual ~IImGuiBridge() = default;

  virtual void begin_frame() = 0;
  virtual void end_frame() = 0;

  static Result<std::unique_ptr<IImGuiBridge>> create(const ImGuiBridgeOptions& options, const EditorContext& context);
};

} // mole
