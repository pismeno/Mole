#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "node_system/inode_manager.hpp"
#include "node_system/manager_reflector_binder.hpp"
#include "node_system/scene.hpp"
#include "utils/result.hpp"
#include "utils/creflector.hpp"

namespace mole {

class AssetManager;
class IFileSystem;
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

  /**
    * @brief Serializes a scene (including its scene_type header) into a writing reflector.
    */
  template <CReflector TReflector>
  requires (TReflector::is_writing())
  void save_scene(
      TReflector& re, 
      const Scene& scene, 
      const ManagerReflectorBinder<TReflector>& binder) const {
    
    // Write scene_type header
    std::string scene_type = scene.scene_type_name;
    serialize(re, "scene_type", scene_type);

    // Gather active nodes
    std::vector<NodeID> active_nodes;
    for (uint32_t i = 0; i < scene.get_highest_allocated_node_index(); ++i) {
      NodeID id = NodeID::from(i, scene.generations[i]);
      if (scene.is_node_valid(id)) {
        active_nodes.push_back(id);
      }
    }

    uint32_t node_count = static_cast<uint32_t>(active_nodes.size());
    re.begin_array("nodes", node_count);

    for (NodeID current : active_nodes) {
      re.begin_object(nullptr);

      UUID saved_uuid = scene.get_uuid(current).value_or(UUID{});
      UUID saved_parent_uuid{};
      
      NodeID parent = scene.get_nodes_parent(current);
      if (scene.is_node_valid(parent)) {
        saved_parent_uuid = scene.get_uuid(parent).value_or(UUID{});
      }

      std::string node_type_str = scene.node_types[current.index()];

      serialize(re, "id", saved_uuid);
      serialize(re, "parent_id", saved_parent_uuid);
      serialize(re, "node_type", node_type_str);

      re.begin_object("managers");
      auto def_it = scene.node_type_definitions.find(node_type_str);
      if (def_it != scene.node_type_definitions.end()) {
        for (INodeManager* mgr : def_it->second.managers) {
          if (const auto* entry = binder.get_entry(mgr->get_name())) {
            re.begin_object(entry->type_id);
            // Note: binder's reflect takes non-const Scene& as INodeResolver, 
            // const_cast is safe here since TReflector::is_writing() is true.
            entry->reflect(mgr, current, re, const_cast<Scene&>(scene));
            re.end_object();
          }
        }
      }
      re.end_object(); // managers
      re.end_object(); // node
    }

    re.end_array(); // nodes
  }

  /**
    * @brief Deserializes a scene from a reading reflector, automatically instantiating
    *        the correct scene type and managers.
    */
  template <CReflector TReflector>
  requires (!TReflector::is_writing())
  Result<std::unique_ptr<Scene>> load_scene(
      TReflector& re, 
      const ManagerReflectorBinder<TReflector>& binder) const {
    
    // Read scene_type first and construct the properly configured Scene
    std::string scene_type;
    serialize(re, "scene_type", scene_type);

    auto scene_res = create_scene(scene_type);
    if (!scene_res) {
      return scene_res;
    }
    std::unique_ptr<Scene> scene = std::move(scene_res).value();

    // Read nodes array
    uint32_t node_count = 0;
    re.begin_array("nodes", node_count);

    struct PendingParent { NodeID child; UUID saved_parent_uuid; };
    std::vector<PendingParent> pending_parents;
    std::unordered_map<UUID, NodeID> uuid_remapping_table;

    for (uint32_t i = 0; i < node_count; ++i) {
      re.begin_object(nullptr);

      UUID saved_uuid{};
      UUID saved_parent_uuid{};
      std::string node_type_str;

      serialize(re, "id", saved_uuid);
      serialize(re, "parent_id", saved_parent_uuid);
      serialize(re, "node_type", node_type_str);

      // Preserve the serialized UUID on the newly created node
      NodeID runtime_node = scene->create_node(NodeID::INVALID, node_type_str, saved_uuid);
      uuid_remapping_table[saved_uuid] = runtime_node;

      if (!saved_parent_uuid.is_nil()) {
        pending_parents.push_back({runtime_node, saved_parent_uuid});
      }

      re.begin_object("managers");
      auto def_it = scene->node_type_definitions.find(node_type_str);
      if (def_it != scene->node_type_definitions.end()) {
        for (INodeManager* mgr : def_it->second.managers) {
          if (const auto* entry = binder.get_entry(mgr->get_name())) {
            re.begin_object(entry->type_id);
            entry->reflect(mgr, runtime_node, re, *scene);
            re.end_object();
          }
        }
      }
      re.end_object(); // managers
      re.end_object(); // node
    }

    re.end_array(); // nodes

    // Reconstruct hierarchy
    for (const auto& pending : pending_parents) {
      auto it = uuid_remapping_table.find(pending.saved_parent_uuid);
      if (it != uuid_remapping_table.end()) {
        scene->reparent_node(pending.child, it->second);
      }
    }

    return scene;
  }


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
