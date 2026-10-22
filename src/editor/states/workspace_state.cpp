#include "editor/states/workspace_state.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "editor/editor_context.hpp"
#include "editor/file_watcher.hpp"
#include "editor/process_orchestrator.hpp"
#include "editor/project.hpp"
#include "editor/project_assets.hpp"
#include "editor/subprocess.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_manager.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

WorkspaceState::WorkspaceState(std::unique_ptr<Project> project)
    : project(std::move(project)),
      file_system(nullptr),
      asset_manager(nullptr),
      process_orchestrator(nullptr) {}

const Project& WorkspaceState::get_project() const {
  return *project;
}

const ProjectAssets& WorkspaceState::get_project_assets() const {
  return *project_assets;
}

Result<std::unique_ptr<WorkspaceState>> WorkspaceState::create(std::unique_ptr<Project> project, const EditorContext& context) {
  std::unique_ptr<WorkspaceState> state(new WorkspaceState(std::move(project)));

  state->project_assets = std::make_unique<ProjectAssets>(context);
  state->process_orchestrator = context.process_orchestrator;
  state->file_system = context.os_context->file_system;
  state->asset_manager = context.resources_context->asset_manager;

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

  // ONLY start the process if files actually changed
  if (fw_events.empty()) return;

  std::vector<std::string> meta_files_command = {"../../tools/python/python.exe", "../../../src/editor/tools/gen_asset_meta_files.py"};
  std::vector<std::string> cook_command = {"../../tools/python/python.exe", "../../../src/editor/tools/cook_assets.py", cache_path.string()};

  bool files_added = false;
  std::vector<std::string> cooked_relative_paths;

  // Get the root location of the project to calculate relative paths
  std::filesystem::path project_root = project->get_location();
  std::filesystem::path assets_root = project_root / "assets"; // Compare against the assets folder

  for (const auto& fw_event : fw_events) {
    if (fw_event.path.extension() == ".meta") continue;

    // 1. Strip the absolute path down to just the filename/subfolders (e.g., test.txt)
    std::filesystem::path rel_path = std::filesystem::relative(fw_event.path, assets_root);

    // 2. Pass just the relative path, since get_cooked_uuids is internally prepending the base assets path
    std::string path_for_uuids = rel_path.generic_string();

    if (fw_event.type == FileWatchEventType::Deleted) {
      auto r_remove = project_assets->remove(path_for_uuids);
      if (!r_remove) {
        std::cerr << "Failed to remove asset " << path_for_uuids << ": " << r_remove.error() << std::endl;
      }
      continue;
    }

    meta_files_command.push_back(fw_event.path.string());
    cook_command.push_back(fw_event.path.string());

    cooked_relative_paths.push_back(path_for_uuids);
    files_added = true;
  }

  if (files_added) {
    // Capture cooked_relative_paths by value so the async callbacks can safely access it later
    auto r_gen_meta_files_process = process_orchestrator->spawn(meta_files_command, [this, cook_command, cooked_relative_paths] {
      auto r_cook_assets_process = process_orchestrator->spawn(cook_command, [this, cooked_relative_paths] {

        // Loop through the paths for the assets we just cooked
        for (const std::string& rel_path : cooked_relative_paths) {
          auto r_get_cooked_uuids = project_assets->get_cooked_uuids(rel_path);

          if (!r_get_cooked_uuids) {
            std::cerr << "Failed to get cooked UUIDs for " << rel_path << ": " << r_get_cooked_uuids.error() << std::endl;
            continue;
          }

          for (const UUID& uuid : r_get_cooked_uuids.value()) {
            auto r_reload = asset_manager->reload(uuid);
            if (!r_reload) {
              std::cerr << "Failed to reload asset " << uuid.to_string() << ": " << r_reload.error() << std::endl;
            } else {
              std::cout << "Reloaded asset " << uuid.to_string() << std::endl;
            }
          }
        }

      });
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
