#pragma once

#include "editor_context.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "editor/project.hpp"

namespace mole {

class ProjectManager {
 public:
  explicit ProjectManager(const EditorContext& context)
  : file_system(context.os_context->file_system) {}
  ~ProjectManager() = default;

  Result<void> create_project(const ProjectOptions& options);
 private:
  std::unique_ptr<Project> active_project;

  IFileSystem* file_system;
};

} // mole