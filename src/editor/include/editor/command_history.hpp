#pragma once

#include <stack>
#include <memory>

#include "editor/icommand.hpp"
#include "utils/result.hpp"

namespace mole {

class CommandHistory {
public:
  explicit CommandHistory() = default;
  
  Result<void> push(std::unique_ptr<ICommand> command);
  Result<void> undo();
  Result<void> redo();
private:
  std::stack<std::unique_ptr<ICommand>> undo_stack;
  std::stack<std::unique_ptr<ICommand>> redo_stack;
};
  
} // mole