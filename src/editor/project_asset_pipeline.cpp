#include "editor/project_asset_pipeline.hpp"

#include <memory>
#include <utility>
#include <iostream>

#include "editor/editor_context.hpp"
#include "editor/file_watcher.hpp"
#include "editor/project.hpp"
#include "editor/project_assets.hpp"
#include "editor/process_orchestrator.hpp"
#include "osal/os_context.hpp"
#include "osal/ifile_system.hpp"
#include "resources/asset_manager.hpp"
#include "resources/material_pool.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"

namespace mole {

ProjectAssetPipeline::ProjectAssetPipeline(const EditorContext& context, Project& project, ProjectAssets& project_assets) :
  project(project),
  project_assets(project_assets),
  file_system(context.os_context->file_system),
  process_orchestrator(context.process_orchestrator),
  asset_manager(context.resources_context->asset_manager),
  material_pool(context.resources_context->material_pool) {};
ProjectAssetPipeline::~ProjectAssetPipeline() { process_orchestrator->flush_and_wait(); };

Result<std::unique_ptr<ProjectAssetPipeline>> ProjectAssetPipeline::create(const EditorContext& context, Project& project, ProjectAssets& project_assets) {
  auto project_asset_pipeline = std::unique_ptr<ProjectAssetPipeline>(new ProjectAssetPipeline(context, project, project_assets));
  
  MOLE_TRY_ASSIGN(project_asset_pipeline->assets_file_watcher, FileWatcher::create("project://assets/", context));

  return std::move(project_asset_pipeline);
}

void ProjectAssetPipeline::update() {
  auto fw_events = assets_file_watcher->poll_events();
  // ONLY start the process if files actually changed
  if (fw_events.empty()) return;

  // Get the root location of the project to calculate relative paths
  std::filesystem::path project_root = project.get_location();
  std::filesystem::path assets_root = project_root / "assets"; // Compare against the assets folder\

  auto r_python_path = file_system->resolve("exe://python/python.exe");
  if (!r_python_path) {
    std::cerr << "Failed to resolve python interpreter: " << r_python_path.error() << std::endl;
    return;
  }

  auto r_meta_script = file_system->resolve("exe://tools/gen_asset_meta_files.py");
  if (!r_meta_script) {
    std::cerr << "Failed to resolve gen_asset_meta_files.py: " << r_meta_script.error() << std::endl;
    return;
  }

  auto r_cook_script = file_system->resolve("exe://tools/cook_assets.py");
  if (!r_cook_script) {
    std::cerr << "Failed to resolve cook_assets.py: " << r_cook_script.error() << std::endl;
    return;
  }

  auto r_resolve_cache = file_system->resolve("assets://");
  assert(r_resolve_cache && "Failed to resolve assets cache, which shouldve been mounted when creating WorkspaceState");
  std::filesystem::path cache_path = r_resolve_cache.value();

  std::vector<std::string> meta_files_command = {r_python_path.value().generic_string(),
                                                 r_meta_script.value().generic_string()};
  std::vector<std::string> cook_command = {r_python_path.value().generic_string(),
                                           r_cook_script.value().generic_string(), cache_path.string()};

  bool files_added = false;
  std::vector<std::string> cooked_relative_paths;
  
  for (const auto& fw_event : fw_events) {
    if (fw_event.path.extension() == ".meta") continue;

    // 1. Strip the absolute path down to just the filename/subfolders (e.g., test.txt)
    std::filesystem::path rel_path = std::filesystem::relative(fw_event.path, assets_root);

    // 2. Pass just the relative path, since get_cooked_uuids is internally prepending the base assets path
    std::string path_for_uuids = rel_path.generic_string();

    if (fw_event.type == EFileWatchEventType::Deleted) {
      auto r_remove = project_assets.remove(path_for_uuids);
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
    auto r_gen_meta_files_process =
        process_orchestrator->spawn(meta_files_command, [this, cook_command, cooked_relative_paths] {
          auto r_cook_assets_process = process_orchestrator->spawn(cook_command, [this, cooked_relative_paths] {
            // Loop through the paths for the assets we just cooked
            for (const std::string& rel_path : cooked_relative_paths) {
              auto r_get_cooked_uuids = project_assets.get_cooked_uuids(rel_path);

              if (!r_get_cooked_uuids) {
                std::cerr << "Failed to get cooked UUIDs for " << rel_path << ": " << r_get_cooked_uuids.error()
                          << std::endl;
                continue;
              }

              for (const UUID& uuid : r_get_cooked_uuids.value()) {
                auto r_reload = asset_manager->reload(uuid);
                if (!r_reload) {
                  std::cerr << "Failed to reload asset " << uuid.to_string() << ": " << r_reload.error() << std::endl;
                } else {
                  std::cout << "Reloaded asset " << uuid.to_string() << std::endl;
                  if (material_pool->is_valid(uuid)) {
                    auto r_reload_mat = material_pool->reload_from_source(uuid);
                    if (r_reload_mat) {
                      std::cout << "Reloaded material " << uuid.to_string() << std::endl;
                    } else {
                      std::cerr << "Failed to reload material " << uuid.to_string() << ": " << r_reload_mat.error() << std::endl;
                    }
                  }
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