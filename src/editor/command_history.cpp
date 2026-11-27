#include "editor/command_history.hpp"
#include <utility>

#include "editor/icommand.hpp"
#include "utils/result.hpp"

namespace mole {

Result<void> CommandHistory::push(std::unique_ptr<ICommand> command) {
  MOLE_TRY(command->execute());
  undo_stack.push(std::move(command));
  redo_stack = {};
  
  return mole::ok();
}

Result<void> CommandHistory::undo() {
  if (undo_stack.empty()) return mole::ok();
  
  MOLE_TRY(undo_stack.top()->undo());
  redo_stack.push(std::move(undo_stack.top()));
  undo_stack.pop();
  
  return mole::ok();
}

Result<void> CommandHistory::redo() {
  if (redo_stack.empty()) return mole::ok();
  
  MOLE_TRY(redo_stack.top()->execute());
  undo_stack.push(std::move(redo_stack.top()));
  redo_stack.pop();

  return mole::ok();
}
  
} // mole