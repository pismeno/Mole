#pragma once

#include "editor/states/ieditor_state.hpp"

#include <memory>

#include "editor/project.hpp"
#include "utils/result.hpp"
#include "osal/ifile_system.hpp"
#include "editor/subprocess.hpp"
#include "editor/process_orchestrator.hpp"

namespace mole {

class WorkspaceState : public IEditorState {
 public:
  ~WorkspaceState() override;

  void update() override;

  static Result<std::unique_ptr<WorkspaceState>> create(std::unique_ptr<Project> project, const EditorContext& context);
 private:
  explicit WorkspaceState(std::unique_ptr<Project> project)
  : project(std::move(project)) {}

  std::vector<std::unique_ptr<Subprocess>> active_processes;

  std::unique_ptr<Project> project;
  std::unique_ptr<FileWatcher> assets_file_watcher;

  IFileSystem* file_system;
  ProcessOrchestrator* process_orchestrator;
};

} // mole