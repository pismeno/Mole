#include "editor/editor.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include "editor/command_history.hpp"
#include "editor/editor_context.hpp"
#include "editor/iim_gui_bridge.hpp"
#include "editor/process_orchestrator.hpp"
#include "editor/project.hpp"
#include "editor/project_assets.hpp"
#include "editor/states/workspace_state.hpp"
#include "editor/subprocess.hpp"
#include "node_system/engine_managers.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/render_gatherer.hpp"
#include "node_system/scene_factory.hpp"
#include "osal/ifile_system.hpp"
#include "osal/iinputs.hpp"
#include "osal/iwindow.hpp"
#include "osal/os_context.hpp"
#include "rendering/bindless_texture_registry.hpp"
#include "rendering/irenderer.hpp"
#include "rendering/material_packer.hpp"
#include "rendering/material_shader_layout_reflector.hpp"
#include "rendering/material_shader_layout_registry.hpp"
#include "rendering/packed_material_cache.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/asset_manager.hpp"
#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/material_types.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"

namespace mole {

Editor::Editor() = default;
Editor::~Editor() = default;

Result<std::unique_ptr<Editor>> Editor::create() {
  auto editor = std::unique_ptr<Editor>(new Editor());

  MOLE_TRY(editor->init_os_context());
  MOLE_TRY(editor->create_window());
  editor->mount_paths();

  MOLE_TRY(editor->init_resources_context());
  MOLE_TRY(editor->init_node_system_context());
  MOLE_TRY(editor->init_rendering_context());
  MOLE_TRY(editor->init_editor_context());

  editor->scene_factory->register_managers<EngineManagers>();

  MOLE_TRY(editor->load_workspace());

  return editor;
}

Result<void> Editor::create_window() {
  WindowOptions window_options = {.logical_width = 800, .logical_height = 600, .title = "Mole Editor"};

  MOLE_TRY_ASSIGN(window, IWindow::create(window_options));

  return mole::ok();
}

void Editor::mount_paths() {
  auto r_mount_core = file_system->mount_from_virtual("core", "exe://engine_data/core");
  assert(r_mount_core && "'exe://' should be mounted when creating filesystem");
  auto r_mount_editor = file_system->mount_from_virtual("editor", "exe://engine_data/editor");
  assert(r_mount_editor && "'exe://' should be mounted when creating filesystem");
}

Result<void> Editor::init_os_context() {
  MOLE_TRY_ASSIGN(file_system, IFileSystem::create());

  os_context = {.file_system = file_system.get()};

  return mole::ok();
}

Result<void> Editor::init_resources_context() {
  resources_context = {
      .os_context = &os_context,
  };

  MOLE_TRY_ASSIGN(asset_manager, AssetManager::create(resources_context));
  resources_context.asset_manager = asset_manager.get();

  material_pool = std::make_unique<MaterialPool>(resources_context);
  resources_context.material_pool = material_pool.get();

  render_gatherer = std::make_unique<RenderGatherer>();

  return mole::ok();
}

Result<void> Editor::init_node_system_context() {
  manager_reflector_binder_factory = std::make_unique<ManagerReflectorBinderFactory<EngineManagers>>();

  node_system_context = {
      .resources_context = &resources_context,
      .render_gatherer = render_gatherer.get(),
      .manager_reflector_binder_factory = manager_reflector_binder_factory.get(),
  };

  MOLE_TRY_ASSIGN(scene_factory, SceneFactory::create(node_system_context));
  node_system_context.scene_factory = scene_factory.get();

  return mole::ok();
}

Result<void> Editor::init_rendering_context() {
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

  MOLE_TRY_ASSIGN(renderer, IRenderer::create(ERendererType::Vulkan, rendering_context));
  rendering_context.renderer = renderer.get();

  MOLE_TRY_ASSIGN(main_presenter, renderer->create_presenter(window.get()));

  return mole::ok();
}

Result<void> Editor::init_editor_context() {
  process_orchestrator = std::make_unique<ProcessOrchestrator>();
  command_history = std::make_unique<CommandHistory>();

  context = {
      .node_system_context = &node_system_context,
      .rendering_context = &rendering_context,
      .os_context = &os_context,
      .resources_context = &resources_context,

      .process_orchestrator = process_orchestrator.get(),
      .command_history = command_history.get()
  };

  ImGuiBridgeOptions bridge_options{
    .renderer_type = ERendererType::Vulkan,
    .native_window_handle = window->get_native_handle(),
    .presenter = main_presenter
  };
  MOLE_TRY_ASSIGN(im_gui_bridge, IImGuiBridge::create(bridge_options, context));
  context.im_gui_bridge = im_gui_bridge.get();

  return mole::ok();
}

Result<void> Editor::load_workspace() {
  std::cout << "Loading workspace..." << std::endl;

  ProjectOptions project_options = {
    .name = "test_project", 
    .location = "test_project"
  };

  MOLE_TRY_ASSIGN(auto project, Project::create(project_options));
  MOLE_TRY_ASSIGN(state, WorkspaceState::create(std::move(project), context));

  return mole::ok();
}

Result<void> Editor::run() {
  IInputs& inputs = window->get_inputs();
  while (!window->should_close()) {
    process_orchestrator->update();
    window->update();

    if (window->is_minimized()) {
      inputs.end_frame();
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      continue;
    }

    MaterialID test_mat_id2 = MaterialID::from(1, 0);

    if (inputs.is_key_pressed(EKeyCode::M)) {
      (void)material_pool->set_property(test_mat_id2, "tint", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    }

    if (inputs.is_key_released(EKeyCode::M)) {
      (void)material_pool->set_property(test_mat_id2, "tint", glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    }

    state->update();

    MOLE_TRY(renderer->begin_frame());
    im_gui_bridge->begin_frame();

    MOLE_TRY(state->render());

    MOLE_TRY(im_gui_bridge->end_frame());
    MOLE_TRY(renderer->end_frame());

    state->end_frame();
    inputs.end_frame();
  }

  return mole::ok();
}

} // mole