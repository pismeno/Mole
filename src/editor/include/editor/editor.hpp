#pragma once

#include <memory>

#include "editor/editor_context.hpp"
#include "node_system/engine_managers.hpp"
#include "node_system/manager_reflector_binder_factory.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/node_types.hpp"
#include "osal/os_context.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"

namespace mole {

class AssetManager;
class BindlessTextureRegistry;
class IEditorState;
class IFileSystem;
class IImGuiBridge;
class IRenderer;
class IWindow;
class MaterialPacker;
class MaterialPool;
class MaterialShaderLayoutReflector;
class MaterialShaderLayoutRegistry;
class PackedMaterialCache;
class ProcessOrchestrator;
class RenderGatherer;
class Scene;
class SceneFactory;

class Editor {
public:
  ~Editor();

  static constexpr float PPU = 32.0f; // TODO move into some sort of settings

  Result<void> run();

  static Result<std::unique_ptr<Editor>> create();

private:
  explicit Editor();

  void mount_paths();
  Result<void> create_window();

  Result<void> init_os_context();
  Result<void> init_resources_context();
  Result<void> init_rendering_context();
  Result<void> init_node_system_context();
  Result<void> init_editor_context();
  Result<void> init_imgui();

  Result<void> load_workspace();

  EditorContext context;
  OSContext os_context;
  ResourcesContext resources_context;
  NodeSystemContext node_system_context;
  RenderingContext rendering_context;

  std::unique_ptr<IFileSystem> file_system;
  std::unique_ptr<AssetManager> asset_manager;
  std::unique_ptr<MaterialPool> material_pool;
  std::unique_ptr<BindlessTextureRegistry> bindless_texture_registry;
  std::unique_ptr<MaterialShaderLayoutReflector> material_shader_layout_reflector;
  std::unique_ptr<MaterialShaderLayoutRegistry> material_shader_layout_registry;
  std::unique_ptr<MaterialPacker> material_packer;
  std::unique_ptr<PackedMaterialCache> packed_material_cache;
  std::unique_ptr<IRenderer> renderer;
  std::unique_ptr<SceneFactory> scene_factory;
  std::unique_ptr<RenderGatherer> render_gatherer;
  std::unique_ptr<ProcessOrchestrator> process_orchestrator;
  std::unique_ptr<ManagerReflectorBinderFactory<EngineManagers>> manager_reflector_binder_factory;

  std::unique_ptr<IWindow> window;
  std::unique_ptr<IEditorState> state;
  std::unique_ptr<IImGuiBridge> imgui_bridge;
};

} // mole