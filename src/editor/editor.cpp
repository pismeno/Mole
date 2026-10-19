#include <editor/editor.hpp>

#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glslang/Public/ShaderLang.h>
#include <glm/gtc/matrix_transform.hpp>

#include <rendering/irenderer.hpp>
#include <osal/iwindow.hpp>
#include <osal/ifile_system.hpp>
#include <resources/asset_manager.hpp>
#include <node_system/scene_factory.hpp>
#include <node_system/managers/drawable_manager.hpp>
#include <node_system/managers/transform_manager.hpp>
#include <resources/asset_types.hpp>

#include "editor/process_orchestrator.hpp"
#include "resources/material_pool.hpp"
#include "editor/subprocess.hpp"
#include "editor/states/workspace_state.hpp"

namespace mole {

Result<void> Editor::init() {
  auto r_create_file_system = IFileSystem::create();
  if (!r_create_file_system) {
    return mole::fail(r_create_file_system.error());
  }
  file_system = std::move(r_create_file_system).value();

  file_system->mount("core", "../../../engine_data/core");
  file_system->mount("editor", "../../../engine_data/editor");

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

  os_context = {
      .file_system = file_system.get()
  };

  resources_context = {
      .os_context = &os_context,
  };

  auto r_create_am = AssetManager::create(resources_context);
  if (!r_create_am) {
    return mole::fail(r_create_am.error());
  }
  asset_manager = std::move(r_create_am).value();
  resources_context.asset_manager = asset_manager.get();

  material_pool = std::make_unique<MaterialPool>();
  resources_context.material_pool = material_pool.get();

  node_system_context = {
      .resources_context = &resources_context,
  };

  auto r_create_sf = SceneFactory::create(node_system_context);
  if (!r_create_sf) return mole::fail(r_create_sf.error());
  scene_factory = std::move(r_create_sf).value();
  node_system_context.scene_factory = scene_factory.get();

  render_gatherer = std::make_unique<RenderGatherer>();
  node_system_context.render_gatherer = render_gatherer.get();

  scene_factory->register_manager<TransformManager>();
  scene_factory->register_manager<DrawableManager>();

  rendering_context = {
      .resources_context = &resources_context
  };

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

      .process_orchestrator = process_orchestrator.get()
  };

  auto r_create_project = Project::create({"test_project", "../../../test_project", context});
  if (!r_create_project) return mole::fail(r_create_project.error());
  std::unique_ptr<Project> project = std::move(r_create_project).value();

  auto r_create_state = WorkspaceState::create(std::move(project), context);
  if (!r_create_state) return mole::fail(r_create_state.error());
  state = std::move(r_create_state).value();

  auto r_get_scene_defs = asset_manager->acquire<TextAsset>(UUID::from_string("83ff4a11-e2af-4609-8c97-9c5b5b508dcb"));
  if (!r_get_scene_defs) return mole::fail(r_get_scene_defs.error());
  AssetHandle<TextAsset> scene_defs = std::move(r_get_scene_defs).value();
  auto r_get_node_defs = asset_manager->acquire<TextAsset>(UUID::from_string("f29fc194-ac5b-4c7e-975e-05b6e4fe971a"));
  if (!r_get_node_defs) return mole::fail(r_get_node_defs.error());
  AssetHandle<TextAsset> node_defs = std::move(r_get_node_defs).value();

  auto r_load_scene_defs = scene_factory->load_scene_definitions_from(scene_defs->as_string_view());
  if (!r_load_scene_defs) return mole::fail(r_load_scene_defs.error());
  auto r_load_node_defs = scene_factory->load_node_definitions_from(node_defs->as_string_view());
  if (!r_load_node_defs) return mole::fail(r_load_node_defs.error());

  auto r_get_texture = asset_manager->acquire<ImageAsset>(UUID::from_string("fd311be5-2c22-47c3-8296-2d35bb926ae7"));
  if (!r_get_texture) return mole::fail(r_get_texture.error());
  AssetHandle<ImageAsset> texture = std::move(r_get_texture).value();
  std::cout << bindless_texture_registry->get_index(texture) << " - Tex index" << std::endl;;

  auto r_get_vert_shader_a = asset_manager->acquire<VertexShaderAsset>(UUID::from_string("4c5999b0-d344-42f9-b588-5a886afa4476"));
  if (!r_get_vert_shader_a) return mole::fail(r_get_vert_shader_a.error());
  AssetHandle<VertexShaderAsset> vert_shader_a = std::move(r_get_vert_shader_a).value();
  auto r_get_frag_shader_a = asset_manager->acquire<FragmentShaderAsset>(UUID::from_string("1780d1ff-552b-4138-8b34-59348152743e"));
  if (!r_get_frag_shader_a) return mole::fail(r_get_frag_shader_a.error());
  AssetHandle<FragmentShaderAsset> frag_shader_a = std::move(r_get_frag_shader_a).value();

  auto r_get_vert_shader = asset_manager->acquire<VertexShaderAsset>(UUID::from_string("3c108d0e-3555-4b6f-95b5-2d721d4595b5"));
  if (!r_get_vert_shader) return mole::fail(r_get_vert_shader.error());
  AssetHandle<VertexShaderAsset> vert_shader_b = std::move(r_get_vert_shader).value();
  auto r_get_frag_shader = asset_manager->acquire<FragmentShaderAsset>(UUID::from_string("c790946f-58f0-4d32-8c06-3327a10c47a5"));
  if (!r_get_frag_shader) return mole::fail(r_get_frag_shader.error());
  AssetHandle<FragmentShaderAsset> frag_shader_b = std::move(r_get_frag_shader).value();

  material_pool->add("test_mat_a", Material{
      .properties = {
          {"test_texture", texture}
          },
      .vertex_shader = std::move(vert_shader_a),
      .fragment_shader = std::move(frag_shader_a),
  });

  material_pool->add("test_mat_b", Material{
      .properties = {
          {"test_texture", texture},
          {"tint", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)}
      },
      .vertex_shader = std::move(vert_shader_b),
      .fragment_shader = std::move(frag_shader_b),
  });


  RendererOptions renderer_options{
      RendererType::Vulkan,
      window->get_native_handle(),
      window->get_framebuffer_width(),
      window->get_framebuffer_height()
  };

  auto renderer_result = IRenderer::create(renderer_options, rendering_context);
  if (!renderer_result) {
    return mole::fail(renderer_result.error());
  }

  renderer = std::move(renderer_result).value();
  rendering_context.renderer = renderer.get();

  return mole::ok();
}

Result<void> Editor::run() {
  auto r_get_texture = asset_manager->acquire<ImageAsset>(UUID::from_string("fd311be5-2c22-47c3-8296-2d35bb926ae7"));
  if (!r_get_texture) return mole::fail(r_get_texture.error());
  AssetHandle<ImageAsset> texture = std::move(r_get_texture).value();

  MaterialID test_mat_id = MaterialID::from(0, 0);
  MaterialID test_mat_id2 = MaterialID::from(1, 0);

  auto r_scene = scene_factory->create_scene("scene_2d");
  if (!r_scene) return mole::fail(r_scene.error());
  scene = std::move(r_scene).value();
  auto* transform_manager = scene->get_manager<TransformManager>();
  auto* drawable_manager = scene->get_manager<DrawableManager>();

  NodeID root_node = scene->create_node(NodeID::INVALID, "sprite_2d");
  NodeID child_node1 = scene->create_node(root_node, "sprite_2d");
  NodeID child_node2 = scene->create_node(root_node, "sprite_2d");

  auto t_1 = transform_manager->set_node_position(root_node, glm::vec2(0.0f, 0.0f));
  if (!t_1) return t_1;
  auto t_2 = transform_manager->set_node_position(child_node1, glm::vec2(1.0f, 1.0f));
  if (!t_2) return t_2;
  auto t_3 = transform_manager->set_node_position(child_node2, glm::vec2(1.0f, 1.5f));
  if (!t_3) return t_3;

  auto r_scale = transform_manager->set_node_scale(root_node, glm::vec2(8.0f, 8.0f));
  if (!r_scale) return r_scale;
  auto r_scale2 = transform_manager->set_node_scale(child_node1, glm::vec2(1.0f, 1.0f));
  if (!r_scale2) return r_scale2;
  auto r_scale3 = transform_manager->set_node_scale(child_node2, glm::vec2(1.0f, 1.0f));
  if (!r_scale3) return r_scale3;

  glm::vec2 world_size = glm::vec2(
      (float)texture->width / PPU,
      (float)texture->height / PPU
  );

  auto r_s_1 = drawable_manager->set_node_size(root_node, world_size);
  if (!r_s_1) return r_s_1;
  auto r_s_2 = drawable_manager->set_node_size(child_node1, world_size);
  if (!r_s_2) return r_s_2;
  auto r_s_3 = drawable_manager->set_node_size(child_node2, world_size);
  if (!r_s_3) return r_s_3;

  auto r_mat_1 = drawable_manager->set_node_material(root_node, test_mat_id);
  if (!r_mat_1) return r_mat_1;
  auto r_mat_2 = drawable_manager->set_node_material(child_node1, test_mat_id);
  if (!r_mat_2) return r_mat_2;
  auto r_mat_3 = drawable_manager->set_node_material(child_node2, test_mat_id2);
  if (!r_mat_3) return r_mat_3;

  auto r_z_1 = drawable_manager->set_node_z_index(root_node, 0);
  if (!r_z_1) return r_z_1;
  auto r_z_2 = drawable_manager->set_node_z_index(child_node1, 1);
  if (!r_z_2) return r_z_2;
  auto r_z_3 = drawable_manager->set_node_z_index(child_node2, 2);
  if (!r_z_3) return r_z_3;

  Inputs& inputs = window->get_inputs();
  while (!window->should_close()) {
    mole::ViewData view_data{};

    state->update();
    process_orchestrator->update();
    window->update();

    float width = static_cast<float>(window->get_framebuffer_width());
    float height = static_cast<float>(window->get_framebuffer_height());

    // FIX 3: Divide screen dimensions by PPU so projection maps to World Units.
    // (Preserving your top-left 0,0 origin style)
    float world_width = width / PPU;
    float world_height = height / PPU;

    auto r_move_to_mouse =  transform_manager->set_node_position(root_node, { inputs.get_mouse_x() / PPU, inputs.get_mouse_y() / PPU });
    if (!r_move_to_mouse) return r_move_to_mouse;

    if (inputs.is_key_pressed(KeyCode::M)) {
      material_pool->set_property(test_mat_id2, "tint", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    }

    if (inputs.is_key_released(KeyCode::M)) {
      material_pool->set_property(test_mat_id2, "tint", glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    }

    view_data.projection = glm::ortho(0.0f, world_width, world_height, 0.0f, -1.0f, 1.0f);
    view_data.view = glm::mat4(1.0f);
    view_data.viewProjection = view_data.projection * view_data.view;

    scene->update();

    renderer->begin_frame();
    renderer->begin_pass(view_data);

    renderer->submit(node_system_context.render_gatherer->gather_render_commands(*scene));

    renderer->end_pass();
    renderer->end_frame();

    scene->end_frame();
    inputs.end_frame();

    if (renderer->needs_framebuffer_resize()) {
      auto r_resize_framebuffer = renderer->resize_framebuffer(
          window->get_framebuffer_width(),
          window->get_framebuffer_height()
      );

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

} // mole