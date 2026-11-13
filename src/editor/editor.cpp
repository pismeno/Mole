#include "editor/editor.hpp"

#include <algorithm> // For std::max
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include "resources/material_types.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glslang/Public/ShaderLang.h>
#include <glm/gtc/matrix_transform.hpp>

#include "editor/editor_context.hpp"
#include "editor/process_orchestrator.hpp"
#include "editor/project.hpp"
#include "editor/project_assets.hpp"
#include "editor/states/workspace_state.hpp"
#include "editor/subprocess.hpp"
#include "node_system/engine_managers.hpp"
#include "node_system/managers/camera_manager.hpp"
#include "node_system/managers/drawable_manager.hpp"
#include "node_system/managers/transform_manager.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/render_gatherer.hpp"
#include "node_system/scene.hpp"
#include "node_system/scene_factory.hpp"
#include "osal/ifile_system.hpp"
#include "osal/inputs.hpp"
#include "osal/iwindow.hpp"
#include "osal/os_context.hpp"
#include "rendering/bindless_texture_registry.hpp"
#include "rendering/irenderer.hpp"
#include "rendering/material_packer.hpp"
#include "rendering/material_shader_layout_reflector.hpp"
#include "rendering/material_shader_layout_registry.hpp"
#include "rendering/packed_material_cache.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/resources_context.hpp"
#include "resources/view_data.hpp"
#include "utils/json_reflector.hpp"
#include "utils/uuid.hpp"

namespace mole {

Editor::Editor() = default;
Editor::~Editor() = default;

Result<void> Editor::init() {
  auto r_create_file_system = IFileSystem::create();
  if (!r_create_file_system) {
    return mole::fail(r_create_file_system.error());
  }
  file_system = std::move(r_create_file_system).value();

  auto r_mount_core = file_system->mount_from_virtual("core", "exe://engine_data/core");
  assert(r_mount_core && "'exe://' should be mounted when creating filesystem");
  auto r_mount_editor = file_system->mount_from_virtual("editor", "exe://engine_data/editor");
  assert(r_mount_editor && "'exe://' should be mounted when creating filesystem");

  glslang::InitializeProcess();

  if (!glfwInit()) {
    return mole::fail("Failed to initialize GLFW");
  }

  auto window_result = IWindow::create(WindowOptions{800, 600, "Mole Editor"});
  if (!window_result) {
    glfwTerminate();
    return mole::fail(window_result.error());
  }
  window = std::move(window_result).value();

  os_context = {.file_system = file_system.get()};

  resources_context = {
      .os_context = &os_context,
  };

  auto r_create_am = AssetManager::create(resources_context);
  if (!r_create_am) {
    return mole::fail(r_create_am.error());
  }
  asset_manager = std::move(r_create_am).value();
  resources_context.asset_manager = asset_manager.get();

  material_pool = std::make_unique<MaterialPool>(resources_context);
  resources_context.material_pool = material_pool.get();

  render_gatherer = std::make_unique<RenderGatherer>();
  manager_reflector_binder_factory = std::make_unique<ManagerReflectorBinderFactory<EngineManagers>>();

  node_system_context = {
      .resources_context = &resources_context,
      .render_gatherer = render_gatherer.get(),
      .manager_reflector_binder_factory = manager_reflector_binder_factory.get(),
  };

  auto r_create_sf = SceneFactory::create(node_system_context);
  if (!r_create_sf) return mole::fail(r_create_sf.error());
  scene_factory = std::move(r_create_sf).value();
  node_system_context.scene_factory = scene_factory.get();

  scene_factory->register_manager<TransformManager>();
  scene_factory->register_manager<DrawableManager>();
  scene_factory->register_manager<CameraManager>();

  rendering_context = {.resources_context = &resources_context};

  bindless_texture_registry = std::make_unique<BindlessTextureRegistry>();
  rendering_context.bindless_texture_registry = bindless_texture_registry.get();
  material_shader_layout_reflector = std::make_unique<MaterialShaderLayoutReflector>();
  rendering_context.material_shader_layout_reflector = material_shader_layout_reflector.get();
  material_shader_layout_registry = std::make_unique<MaterialShaderLayoutRegistry>(rendering_context);
  rendering_context.material_shader_layout_registry = material_shader_layout_registry.get();
  material_packer = std::make_unique<MaterialPacker>(rendering_context);
  rendering_context.material_packer = material_packer.get();
  packed_material_cache = std::make_unique<PackedMaterialCache>(rendering_context);
  rendering_context.packed_material_cache = packed_material_cache.get();

  process_orchestrator = std::make_unique<ProcessOrchestrator>();

  context = {
      .node_system_context = &node_system_context,
      .rendering_context = &rendering_context,
      .os_context = &os_context,
      .resources_context = &resources_context,

      .process_orchestrator = process_orchestrator.get(),
  };

  RendererOptions renderer_options{ERendererType::Vulkan, window->get_native_handle(), window->get_framebuffer_width(),
                                   window->get_framebuffer_height()};

  auto renderer_result = IRenderer::create(renderer_options, rendering_context);
  if (!renderer_result) {
    return mole::fail(renderer_result.error());
  }

  renderer = std::move(renderer_result).value();
  rendering_context.renderer = renderer.get();

  // Instantly load the workspace during initialization
  auto r_load = load_workspace();
  if (!r_load) {
    return mole::fail("Failed to load workspace: " + r_load.error());
  }

  return mole::ok();
}

Result<void> Editor::load_workspace() {
  std::cout << "Loading workspace..." << std::endl;

  auto r_create_project = Project::create({"test_project", "test_project", context});
  if (!r_create_project) return mole::fail(r_create_project.error());
  std::unique_ptr<Project> project = std::move(r_create_project).value();

  auto r_create_state = WorkspaceState::create(std::move(project), context);
  if (!r_create_state) return mole::fail(r_create_state.error());
  WorkspaceState* workspace_state = r_create_state.value().get();
  state = std::move(r_create_state).value();

  auto r_res_uuid = workspace_state->get_project_assets().resolve("scene_definitions.json", EAssetType::Text);
  if (!r_res_uuid) return mole::fail(r_res_uuid.error());
  auto r_get_scene_defs = asset_manager->acquire<TextAsset>(r_res_uuid.value());
  if (!r_get_scene_defs) return mole::fail(r_get_scene_defs.error());
  AssetHandle<TextAsset> scene_defs = std::move(r_get_scene_defs).value();

  r_res_uuid = workspace_state->get_project_assets().resolve("node_definitions.json", EAssetType::Text);
  if (!r_res_uuid) return mole::fail(r_res_uuid.error());
  auto r_get_node_defs = asset_manager->acquire<TextAsset>(r_res_uuid.value());
  if (!r_get_node_defs) return mole::fail(r_get_node_defs.error());
  AssetHandle<TextAsset> node_defs = std::move(r_get_node_defs).value();

  auto r_load_scene_defs = scene_factory->load_scene_definitions_from(scene_defs->as_string_view());
  if (!r_load_scene_defs) return mole::fail(r_load_scene_defs.error());
  auto r_load_node_defs = scene_factory->load_node_definitions_from(node_defs->as_string_view());
  if (!r_load_node_defs) return mole::fail(r_load_node_defs.error());

  auto r_scene = scene_factory->create_scene("scene_2d");
  if (!r_scene) return mole::fail(r_scene.error());
  scene = std::move(r_scene).value();

  auto* transform_manager = scene->get_manager<TransformManager>();
  auto* drawable_manager = scene->get_manager<DrawableManager>();
  auto* camera_manager = scene->get_manager<CameraManager>();
  
  auto r_uuid = workspace_state->get_project_assets().resolve("scene.scene.json", EAssetType::Scene);
  if (!r_uuid) return mole::fail(r_uuid.error());
  auto r_scene_asset = asset_manager->acquire<SceneAsset>(r_uuid.value());
  if (!r_scene_asset) return mole::fail(r_scene_asset.error());
  auto r_instantiate = scene_factory->instantiate_scene(r_scene_asset.value());
  if (!r_instantiate) {
    std::cerr << "failed to load scene: " << r_instantiate.error() << std::endl;
  } else {
    scene = std::move(r_instantiate).value();
    std::cout << "loaded scene successfuly" << std::endl;
  }

  return mole::ok();
}

Result<void> Editor::run() {
  IInputs& inputs = window->get_inputs();
  while (!window->should_close()) {
    mole::ViewData view_data{};

    if (state) {
      state->update();
    }
    process_orchestrator->update();
    window->update();

    if (window->is_minimized()) {
      inputs.end_frame();
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      continue;
    }

    if (renderer->needs_framebuffer_resize()) {
      auto r_resize_framebuffer =
          renderer->resize_framebuffer(window->get_framebuffer_width(), window->get_framebuffer_height());

      if (!r_resize_framebuffer) {
        return mole::fail(r_resize_framebuffer.error());
      }
    }

    if (inputs.is_key_pressed(EKeyCode::J)) {
      if (scene) {
        nlohmann::json json;
        JsonWriteReflector writer(json);
        scene_factory->save_scene(writer, *scene);
        std::cout << json.dump(2) << std::endl;
      }
    }

    if (state) {
      MaterialID test_mat_id2 = MaterialID::from(1, 0);

      if (inputs.is_key_pressed(EKeyCode::M)) {
        (void)material_pool->set_property(test_mat_id2, "tint", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
      }

      if (inputs.is_key_released(EKeyCode::M)) {
        (void)material_pool->set_property(test_mat_id2, "tint", glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
      }

      scene->update();
    }

    float width = static_cast<float>(window->get_framebuffer_width());
    float height = static_cast<float>(std::max(1u, window->get_framebuffer_height()));

    // --- CAMERA CALCULATION ---
    if (scene) {
      auto* camera_manager = scene->get_manager<CameraManager>();
      auto* transform_manager = scene->get_manager<TransformManager>();

      NodeID active_cam = camera_manager->get_active_camera_node();
      if (active_cam != NodeID::INVALID) {
        auto r_cam = camera_manager->get_camera(active_cam);
        if (r_cam) {
          Camera cam = r_cam.value();

          // Dynamically adjust aspect ratio based on window size
          cam.aspect_ratio = width / height;
          auto r_set_cam = camera_manager->set_camera(active_cam, cam); // Write it back
          if (!r_set_cam) return r_set_cam;

          float half_h = (cam.size / cam.zoom) * 0.5f;
          float half_w = half_h * cam.aspect_ratio;

          // Vulkan projection (orthoZO), Y-axis swap applied via half_h parameters
          view_data.projection = glm::orthoZO(-half_w, half_w, half_h, -half_h, cam.near_plane, cam.far_plane);

          glm::mat4 view = glm::mat4(1.0f);

          // Retrieve position from transform manager if implemented
          glm::vec2 cam_pos{0.0f, 0.0f};
          // Note: Adjust the exact getter name based on your TransformManager implementation
          // if (auto r_pos = transform_manager->get_node_position(active_cam)) cam_pos = r_pos.value();

          // Transform logic (Rotation would go here if needed)
          view = glm::translate(view, glm::vec3(-cam_pos.x, -cam_pos.y, 0.0f));

          view_data.view = view;
          view_data.viewProjection = view_data.projection * view_data.view;
        }
      } else {
        // Fallback if scene is loaded but no camera is active
        float world_width = width / PPU;
        float world_height = height / PPU;
        view_data.projection = glm::ortho(0.0f, world_width, world_height, 0.0f, -1.0f, 1.0f);
        view_data.view = glm::mat4(1.0f);
        view_data.viewProjection = view_data.projection * view_data.view;
      }
    } else {
      // Fallback before the workspace is loaded via 'L' (removed, but kept for safety)
      view_data.projection = glm::mat4(1.0f);
      view_data.view = glm::mat4(1.0f);
      view_data.viewProjection = view_data.projection * view_data.view;
    }
    // --------------------------

    renderer->begin_frame();
    renderer->begin_pass(view_data);

    if (scene) {
      renderer->submit(node_system_context.render_gatherer->gather_render_commands(*scene));
    }

    renderer->end_pass();
    renderer->end_frame();

    if (scene) {
      scene->end_frame();
    }
    inputs.end_frame();

    if (renderer->needs_framebuffer_resize()) {
      auto r_resize_framebuffer =
          renderer->resize_framebuffer(window->get_framebuffer_width(), window->get_framebuffer_height());

      if (!r_resize_framebuffer) {
        return mole::fail(r_resize_framebuffer.error());
      }
    }

    if (renderer->is_context_lost()) {
      return mole::fail("Renderer context lost");
    }
  }

  return mole::ok();
}

Result<void> Editor::shutdown() {
  if (window) {
    auto window_close_result = window->close();
    if (!window_close_result) {
      std::cerr << "Error closing window: " << window_close_result.error() << std::endl;
    }
    window.reset();
  }

  glfwTerminate();
  glslang::FinalizeProcess();

  return mole::ok();
}

} // namespace mole
