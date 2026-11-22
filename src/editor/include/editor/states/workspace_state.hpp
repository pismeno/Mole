#pragma once

#include "editor/states/ieditor_state.hpp"

#include <memory>
#include <string_view>

#include "resources/view_data.hpp"
#include "utils/result.hpp"

namespace mole {

struct EditorContext;
class Project;
class ProjectAssets;
class ProjectAssetPipeline;
class Scene;
class AssetManager;
class SceneFactory;
class RenderGatherer;
class IRenderer;

class WorkspaceState : public IEditorState {
public:
  ~WorkspaceState() override;

  static constexpr float PPU = 32.0f; // TODO move into some sort of settings

  void update() override;
  void render(IRenderer* renderer) override;
  void end_frame() override;

  const Project& get_project() const;
  const ProjectAssets& get_project_assets() const;

  static Result<std::unique_ptr<WorkspaceState>> create(std::unique_ptr<Project> project, const EditorContext& context);

private:
  explicit WorkspaceState(const EditorContext& context);

  Result<void> load_scene(std::string_view path);
  ViewData calculate_view_data(IRenderer* renderer);
  ViewData calculate_view_data(float width, float height);

  std::unique_ptr<Project> project;
  std::unique_ptr<ProjectAssets> project_assets;
  std::unique_ptr<ProjectAssetPipeline> project_asset_pipeline;

  std::unique_ptr<Scene> scene;

  AssetManager* asset_manager;
  SceneFactory* scene_factory;
  RenderGatherer* render_gatherer;
};

} // namespace mole
