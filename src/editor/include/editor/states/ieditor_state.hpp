#pragma once

#include "utils/result.hpp"

namespace mole {

class IEditorState {
public:
  virtual ~IEditorState() = default;

  virtual void update() = 0;
  /** @brief Renders the state (its own render targets + ImGui windows). Forwards fatal renderer errors; a skipped pass is ok. */
  virtual Result<void> render() = 0;
  virtual void end_frame() = 0;
};

} // mole