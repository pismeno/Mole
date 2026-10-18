#pragma once

#include <memory>

#include "osal/ifile_system.hpp"
#include "utils/result.hpp"

namespace mole {

struct ProjectOptions {
  std::string name;
};

class Project {
 public:
  ~Project() = default;

  inline const std::string_view get_name() const { return name; }

  static Result<std::unique_ptr<Project>> create(const ProjectOptions& options);
 private:
  explicit Project() = default;

  std::string name;
};

} // mole