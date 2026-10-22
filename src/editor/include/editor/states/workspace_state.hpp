#pragma once

#include "editor/states/ieditor_state.hpp"

#include <memory>
#include <vector>

#include "utils/result.hpp"

namespace mole {

struct EditorContext;
class AssetManager;
class FileWatcher;
class IFileSystem;
class ProcessOrchestrator;
class Project;
class ProjectAssets;
class Subprocess;

class WorkspaceState : public IEditorState {
 public:
  ~WorkspaceState() override;

  void update() override;

  [[nodiscard]] const Project& get_project() const;
  [[nodiscard]] const ProjectAssets& get_project_assets() const;

  static Result<std::unique_ptr<WorkspaceState>> create(std::unique_ptr<Project> project, const EditorContext& context);

 private:
  explicit WorkspaceState(std::unique_ptr<Project> project);

  std::vector<std::unique_ptr<Subprocess>> active_processes;

  std::unique_ptr<Project> project;
  std::unique_ptr<ProjectAssets> project_assets;
  std::unique_ptr<FileWatcher> assets_file_watcher;

  IFileSystem* file_system;
  AssetManager* asset_manager;
  ProcessOrchestrator* process_orchestrator;
};

} // mole