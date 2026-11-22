#pragma once

namespace mole {

class IRenderer;
  
class IEditorState {
public:
  virtual ~IEditorState() = default;

  virtual void update() = 0;
  virtual void render(IRenderer* renderer) = 0;
  virtual void end_frame() = 0;
};

} // mole