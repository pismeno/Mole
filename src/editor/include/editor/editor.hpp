#pragma once

#include <memory>

#include "editor/editor_context.hpp"
#include "node_system/node_system_context.hpp"
#include "osal/os_context.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"

namespace mole {

class AssetManager;
class BindlessTextureRegistry;
class IEditorState;
class IFileSystem;
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
  static constexpr float PPU = 32.0f; // TODO move into some sort of settings

  explicit Editor();
  ~Editor();

  Result<void> init();
  Result<void> run();
  Result<void> shutdown();

 private:
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

  std::unique_ptr<IWindow> window;
  std::unique_ptr<Scene> scene;
  std::unique_ptr<IEditorState> state;
};

} // mole
