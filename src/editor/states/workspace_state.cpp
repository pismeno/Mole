#include "editor/states/workspace_state.hpp"

#include <cassert>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include "editor/editor_context.hpp"
#include "editor/file_watcher.hpp"
#include "editor/process_orchestrator.hpp"
#include "editor/project.hpp"
#include "editor/project_asset_pipeline.hpp"
#include "editor/project_assets.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_manager.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"

namespace mole {

WorkspaceState::WorkspaceState() = default;
WorkspaceState::~WorkspaceState() = default;

const Project& WorkspaceState::get_project() const { return *project; }

const ProjectAssets& WorkspaceState::get_project_assets() const { return *project_assets; }

Result<std::unique_ptr<WorkspaceState>> WorkspaceState::create(std::unique_ptr<Project> project, const EditorContext& context) {
  std::unique_ptr<WorkspaceState> state(new WorkspaceState());

  state->project = std::move(project);
  state->project_assets = std::make_unique<ProjectAssets>(context);

  IFileSystem* file_system = context.os_context->file_system;

  std::filesystem::path project_location = state->project->get_location();
  file_system->mount("project", project_location);
  file_system->mount("assets", project_location / "cache");

  MOLE_TRY_ASSIGN(state->project_asset_pipeline, ProjectAssetPipeline::create(context, *state->project, *state->project_assets));

  auto r_python_path = file_system->resolve("exe://../../tools/python/python.exe");
  if (!r_python_path) return mole::fail("Failed to resolve python interpreter: " + r_python_path.error());

  auto r_sync_script = file_system->resolve("exe://../../../../src/editor/tools/sync_assets.py");
  if (!r_sync_script) return mole::fail("Failed to resolve sync_assets.py: " + r_sync_script.error());

  auto r_spawn_sync = context.process_orchestrator->spawn(
      {r_python_path.value().generic_string(), r_sync_script.value().generic_string(),
       (project_location / "assets").generic_string(), (project_location / "cache").generic_string()});
  if (!r_spawn_sync) return mole::fail("Failed to spawn sync_assets.py: " + r_spawn_sync.error());

  context.process_orchestrator->flush_and_wait(); // wait for asset sync before continuing

  return std::move(state);
}

void WorkspaceState::update() {
  project_asset_pipeline->update();
}

} // namespace mole
