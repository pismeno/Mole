#pragma once

#include "editor/states/ieditor_state.hpp"

#include <memory>

#include "utils/result.hpp"

namespace mole {

struct EditorContext;
class Project;
class ProjectAssets;
class ProjectAssetPipeline;

class WorkspaceState : public IEditorState {
public:
  ~WorkspaceState() override;

  void update() override;

  [[nodiscard]] const Project& get_project() const;
  [[nodiscard]] const ProjectAssets& get_project_assets() const;

  static Result<std::unique_ptr<WorkspaceState>> create(std::unique_ptr<Project> project, const EditorContext& context);

private:
  explicit WorkspaceState();

  std::unique_ptr<Project> project;
  std::unique_ptr<ProjectAssets> project_assets;
  std::unique_ptr<ProjectAssetPipeline> project_asset_pipeline;
};

} // namespace mole
