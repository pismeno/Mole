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

  state->file_system = context.os_context->file_system;
  std::filesystem::path project_location = state->project->get_location();
  state->file_system->mount("project", project_location);
  state->file_system->mount("assets",  project_location / "cache");

  auto r_create_file_watcher = FileWatcher::create("project://assets/", context);
  if (!r_create_file_watcher) return mole::fail(r_create_file_watcher.error());
  state->assets_file_watcher = std::move(r_create_file_watcher).value();

  return std::move(state);
}

void WorkspaceState::update() {
  auto fw_events = assets_file_watcher->poll_events();

  //auto r_resolve_cache = file_system->resolve("assets://");
  //assert(r_resolve_cache && "Failed to resolve assets cache, which shouldve been mounted when creating WorkspaceState");
  //std::filesystem::path cache_path = r_resolve_cache.value();

  // 3. ONLY start the process if files actually changed
  if (!fw_events.empty()) {
    std::vector<std::string> command = {"../../tools/python/python.exe", "../../../src/editor/tools/gen_asset_meta_files.py"};
    bool files_added = false;

    for (const auto& fw_event : fw_events) {
      if (fw_event.type == FileWatchEventType::Deleted || fw_event.path.extension() == ".meta") continue;

      command.push_back(fw_event.path.string());
      files_added = true;
    }

    if (files_added) {
      auto r_gen_meta_files_process = Subprocess::create(command);
      if (!r_gen_meta_files_process) {
        std::cerr << "Failed to start gen_meta_files_process" << std::endl;
      } else {
        // MOVE the process into the persistent list so it survives this frame
        active_processes.push_back(std::move(r_gen_meta_files_process).value());
      }
    }
  }

  // 4. Poll and clean up active processes every frame
  for (auto it = active_processes.begin(); it != active_processes.end(); ) {
    auto& process = *it;

    // Always drain the output to prevent pipe blocking
    for (const std::string& line : process->poll_output()) {
      std::cout << line << "\n";
    }

    // If it finished, remove it from the list (which destroys the object cleanly)
    if (!process->is_running()) {
      it = active_processes.erase(it);
    } else {
      ++it;
    }
  }
}

} // mole