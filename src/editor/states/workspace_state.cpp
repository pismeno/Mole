#include "editor/states/workspace_state.hpp"

#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "utils/result.hpp"
#include "editor/file_watcher.hpp"
#include "editor/editor_context.hpp"
#include "editor/project.hpp"
#include "editor/subprocess.hpp"

namespace mole {

Result<std::unique_ptr<WorkspaceState>> WorkspaceState::create(std::unique_ptr<Project> project, const EditorContext& context) {
  std::unique_ptr<WorkspaceState> state(new WorkspaceState(std::move(project)));

  state->process_orchestrator = context.process_orchestrator;
  state->file_system = context.os_context->file_system;

  std::filesystem::path project_location = state->project->get_location();
  state->file_system->mount("project", project_location);
  state->file_system->mount("assets",  project_location / "cache");

  auto r_create_file_watcher = FileWatcher::create("project://assets/", context);
  if (!r_create_file_watcher) return mole::fail(r_create_file_watcher.error());
  state->assets_file_watcher = std::move(r_create_file_watcher).value();

  return std::move(state);
}

WorkspaceState::~WorkspaceState() {
  process_orchestrator->flush_and_wait();
}

void WorkspaceState::update() {
  auto fw_events = assets_file_watcher->poll_events();

  auto r_resolve_cache = file_system->resolve("assets://");
  assert(r_resolve_cache && "Failed to resolve assets cache, which shouldve been mounted when creating WorkspaceState");
  std::filesystem::path cache_path = r_resolve_cache.value();

  // 3. ONLY start the process if files actually changed
  if (fw_events.empty()) return;

  std::vector<std::string> meta_files_command = {"../../tools/python/python.exe", "../../../src/editor/tools/gen_asset_meta_files.py"};
  std::vector<std::string> cook_command = {"../../tools/python/python.exe", "../../../src/editor/tools/cook_assets.py", cache_path.string()};

  bool files_added = false;
  for (const auto& fw_event : fw_events) {
    if (fw_event.type == FileWatchEventType::Deleted || fw_event.path.extension() == ".meta") continue;

      meta_files_command.push_back(fw_event.path.string());
      cook_command.push_back(fw_event.path.string());
      files_added = true;
    }

  if (files_added) {
    auto r_gen_meta_files_process = process_orchestrator->spawn(meta_files_command, [this, cook_command] {
      auto r_cook_assets_process = process_orchestrator->spawn(cook_command);
      if (!r_cook_assets_process) {
        std::cerr << "Failed to start cook_assets_process" << std::endl;
      }
    });
    if (!r_gen_meta_files_process) {
      std::cerr << "Failed to start gen_meta_files_process" << std::endl;
    }
  }
}

} // mole