#pragma once

namespace mole {

class IEditorState {
public:
  virtual ~IEditorState() = default;

  virtual void update() = 0;
};

} // mole