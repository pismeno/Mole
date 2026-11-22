#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "editor/states/workspace_state.hpp"

#include <algorithm> // For std::max
#include <cassert>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "editor/editor_context.hpp"
#include "editor/file_watcher.hpp"
#include "editor/process_orchestrator.hpp"
#include "editor/project.hpp"
#include "editor/project_asset_pipeline.hpp"
#include "editor/project_assets.hpp"
#include "node_system/managers/camera_manager.hpp"
#include "node_system/managers/transform_manager.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/render_gatherer.hpp"
#include "node_system/scene.hpp"
#include "node_system/scene_factory.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "rendering/irenderer.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/resources_context.hpp"
#include "resources/view_data.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

WorkspaceState::WorkspaceState(const EditorContext& context) : 
  asset_manager(context.resources_context->asset_manager),
  scene_factory(context.node_system_context->scene_factory),
  render_gatherer(context.node_system_context->render_gatherer) {}
WorkspaceState::~WorkspaceState() = default;

const Project& WorkspaceState::get_project() const { return *project; }

const ProjectAssets& WorkspaceState::get_project_assets() const { return *project_assets; }

Result<std::unique_ptr<WorkspaceState>> WorkspaceState::create(std::unique_ptr<Project> project, const EditorContext& context) {
  std::unique_ptr<WorkspaceState> state(new WorkspaceState(context));

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

  MOLE_TRY_ASSIGN(UUID scene_defs_uuid, state->project_assets->resolve("scene_definitions.json", EAssetType::Text)); // TODO remove ties to workspace/project
  MOLE_TRY_ASSIGN(AssetHandle<TextAsset> scene_defs, state->asset_manager->acquire<TextAsset>(scene_defs_uuid));

  MOLE_TRY_ASSIGN(UUID node_defs_uuid, state->project_assets->resolve("node_definitions.json", EAssetType::Text)); // TODO remove ties to workspace/project
  MOLE_TRY_ASSIGN(AssetHandle<TextAsset> node_defs, state->asset_manager->acquire<TextAsset>(node_defs_uuid));

  MOLE_TRY(state->scene_factory->load_scene_definitions_from(scene_defs->as_string_view()));
  MOLE_TRY(state->scene_factory->load_node_definitions_from(node_defs->as_string_view()));

  MOLE_TRY(state->load_scene("scene.scene.json")); // TODO get current scene from some project_manifest.json

  return std::move(state);
}

void WorkspaceState::update() {
  project_asset_pipeline->update();
  if (scene) scene->update();
}

void WorkspaceState::render(IRenderer* renderer) {
  ImGui::Begin("Hello, World!");
  ImGui::Text("Hello, World from Mole Editor!");
  static int counter = 0;
  if (ImGui::Button("Click Me!")) {
    counter++;
  }
  ImGui::Text("Button clicked %d times", counter);
  ImGui::End();
  
  renderer->begin_pass(calculate_view_data(renderer));
  if (scene) renderer->submit(render_gatherer->gather_render_commands(*scene));
  renderer->end_pass();
}

void WorkspaceState::end_frame() {
  if (scene) scene->end_frame();
}

Result<void> WorkspaceState::load_scene(std::string_view path) {
  MOLE_TRY_ASSIGN(UUID scene_asset_uuid, project_assets->resolve(path, EAssetType::Scene));
  MOLE_TRY_ASSIGN(AssetHandle<SceneAsset> scene_asset, asset_manager->acquire<SceneAsset>(scene_asset_uuid));
  MOLE_TRY_ASSIGN(scene, scene_factory->instantiate_scene(scene_asset));

  return mole::ok();
}

ViewData WorkspaceState::calculate_view_data(IRenderer* renderer) {
  float width = static_cast<float>(renderer->get_framebuffer_width());
  float height = static_cast<float>((std::max)(1u, renderer->get_framebuffer_height()));
  
  return calculate_view_data(width, height);
}

ViewData WorkspaceState::calculate_view_data(float width, float height) {
  ViewData view_data{};

  if (!scene) return view_data;
  
  auto* camera_manager = scene->get_manager<CameraManager>();
  auto* transform_manager = scene->get_manager<TransformManager>();
  (void)transform_manager;

  NodeID active_cam = camera_manager->get_active_camera_node();
  if (active_cam == NodeID::INVALID) {
    float world_width = width / PPU;
    float world_height = height / PPU;
    view_data.projection = glm::ortho(0.0f, world_width, world_height, 0.0f, -1.0f, 1.0f);
    view_data.view = glm::mat4(1.0f);
    view_data.viewProjection = view_data.projection * view_data.view;

    return view_data;
  }
  auto r_cam = camera_manager->get_camera(active_cam);
  
  if (!r_cam) return view_data;
  
  Camera cam = r_cam.value();

  // Dynamically adjust aspect ratio based on window size
  cam.aspect_ratio = width / height;
  auto r_set_cam = camera_manager->set_camera(active_cam, cam); // Write it back
  (void)r_set_cam;

  float half_h = (cam.size / cam.zoom) * 0.5f;
  float half_w = half_h * cam.aspect_ratio;

  // Vulkan projection (orthoZO), Y-axis swap applied via half_h parameters
  view_data.projection = glm::orthoZO(-half_w, half_w, half_h, -half_h, cam.near_plane, cam.far_plane);

  glm::mat4 view = glm::mat4(1.0f);

  // Retrieve position from transform manager if implemented
  glm::vec2 cam_pos{0.0f, 0.0f};

  // Transform logic (Rotation would go here if needed)
  view = glm::translate(view, glm::vec3(-cam_pos.x, -cam_pos.y, 0.0f));

  view_data.view = view;
  view_data.viewProjection = view_data.projection * view_data.view;

  return view_data;
}

} // mole