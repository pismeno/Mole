#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "node_system/inode_manager.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

class AssetManager;
class IFileSystem;
class Scene;
struct NodeSystemContext;

using ManagerFactory = std::function<std::unique_ptr<INodeManager>()>;

/**
 * @brief Factory class for creating scenes.
 */
class SceneFactory {
 public:
  ~SceneFactory();

  /**
   * @brief Loads scene definitions from a json source, into this instance of SceneFactory.
   */
  Result<void> load_scene_definitions_from(const std::string_view definitions_source);

  /**
   * @brief Loads node definitions from a json source, into this instance of SceneFactory.
   */
  Result<void> load_node_definitions_from(const std::string_view definitions_source);

  /**
   * @brief Creates a scene of the given type, this method looks up scene types in this instance of SceneFactory.
   * @param scene_type the string id of the scene type to create.
   */
  Result<std::unique_ptr<Scene>> create_scene(const std::string& scene_type) const;
  Result<std::unique_ptr<Scene>> load_scene_from(const std::string_view scene_source) const;
  Result<std::unique_ptr<Scene>> load_scene_from_asset(UUID uuid) const;
  Result<void> save_scene_to(const Scene& scene, std::string_view path) const;

  /**
   * @brief Registers a manager for this SceneFactory instance.
   * @note Asserts that the manager name must be unique.
   */
  template <typename T>
  requires std::derived_from<T, INodeManager> && std::default_initializable<T>
  void register_manager() {
    register_manager_factory(T::TYPE_ID, []() -> std::unique_ptr<INodeManager> {
      return std::make_unique<T>();
    });
  }

  void register_manager_factory(std::string_view name, ManagerFactory factory);

  /**
   * @brief Creates a SceneFactory instance.
   */
  static Result<std::unique_ptr<SceneFactory>> create(const NodeSystemContext& context);

 private:
  explicit SceneFactory() = default;

  Result<void> init(const NodeSystemContext& context);

  struct NodeDefinition {
    std::vector<std::string> managers;
  };

  struct SceneDefinition {
    std::vector<std::string> managers;
    std::vector<std::string> allowed_nodes;
  };

  std::unordered_map<std::string, ManagerFactory> manager_registry;
  std::unordered_map<std::string, NodeDefinition> node_definitions;
  std::unordered_map<std::string, SceneDefinition> scene_definitions;

  AssetManager* asset_manager = nullptr;
  IFileSystem* file_system = nullptr;
};

} // mole
