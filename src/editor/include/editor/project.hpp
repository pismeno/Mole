#pragma once

#include <memory>

#include "utils/result.hpp"
#include "file_watcher.hpp"
#include "editor/editor_context.hpp"

namespace mole {

struct ProjectOptions {
  std::string name;

  const EditorContext& context;
};

class Project {
 public:
  ~Project() = default;

  inline const std::string_view get_name() const { return name; }

  static Result<std::unique_ptr<Project>> create(const ProjectOptions& options);
 private:
  explicit Project() = default;

  std::string name;

  std::unique_ptr<FileWatcher> assets_file_watcher;
};

} // mole