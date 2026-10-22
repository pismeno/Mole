#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "utils/result.hpp"

namespace mole {

struct EditorContext;

struct ProjectOptions {
  std::string name;
  std::filesystem::path location;

  const EditorContext& context;
};

class Project {
 public:
  ~Project() = default;

  inline const std::string_view get_name() const { return name; }
  inline const std::filesystem::path& get_location() const { return location; }

  static Result<std::unique_ptr<Project>> create(const ProjectOptions& options);
 private:
  explicit Project() = default;

  std::string name;
  std::filesystem::path location;
};

} // mole
