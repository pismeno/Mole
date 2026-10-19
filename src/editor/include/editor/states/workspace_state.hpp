#pragma once

#include "editor/states/ieditor_state.hpp"

#include <memory>

#include "editor/project.hpp"
#include "utils/result.hpp"
#include "osal/ifile_system.hpp"
#include "editor/subprocess.hpp"

namespace mole {

class WorkspaceState : public IEditorState {
 public:
  void update() override;

  static Result<std::unique_ptr<WorkspaceState>> create(std::unique_ptr<Project> project, const EditorContext& context);
 private:
  explicit WorkspaceState(std::unique_ptr<Project> project)
  : project(std::move(project)) {}

  std::vector<std::unique_ptr<Subprocess>> active_processes;

  std::unique_ptr<Project> project;
  std::unique_ptr<FileWatcher> assets_file_watcher;

  IFileSystem* file_system;
};

} // mole