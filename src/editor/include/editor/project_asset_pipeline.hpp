#pragma once

#include <memory>

#include "editor/editor_context.hpp"
#include "editor/file_watcher.hpp"
#include "utils/result.hpp"

namespace mole {

class FileWatcher;
class Project;
class ProjectAssets;
class IFileSystem;
class ProcessOrchestrator;
class AssetManager;
class MaterialPool;
struct EditorContext;
  
class ProjectAssetPipeline {
public:
  ~ProjectAssetPipeline();

  void update();
  
  static Result<std::unique_ptr<ProjectAssetPipeline>> create(const EditorContext& context, Project& project, ProjectAssets& project_assets);
private:
  explicit ProjectAssetPipeline(const EditorContext& context, Project& project, ProjectAssets& project_assets);

  std::unique_ptr<FileWatcher> assets_file_watcher;

  Project& project;
  ProjectAssets& project_assets;

  IFileSystem* file_system;
  ProcessOrchestrator* process_orchestrator; 
  AssetManager* asset_manager;
  MaterialPool* material_pool;
};
  
} // mole

