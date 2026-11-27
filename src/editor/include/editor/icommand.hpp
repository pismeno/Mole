#pragma once

#include "utils/result.hpp"

namespace mole {

class ICommand {
public:
  virtual ~ICommand() = default;
  virtual Result<void> execute() = 0;
  virtual Result<void> undo() = 0;
};
  
} // mole