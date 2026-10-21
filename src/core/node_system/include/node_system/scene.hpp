#pragma once

#include "node_types.hpp"
#include "node.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <span>

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include <utils/result.hpp>
#include <utils/string_hash.hpp>
#include <node_system/inode_manager.hpp>

namespace mole {

class Scene {

  friend class NodeHandle;

 public:

  explicit Scene(std::string scene_type_name) : scene_type_name(std::move(scene_type_name)) {
    nodes.resize(MAX_ACTIVE_NODES);
    generations.resize(MAX_ACTIVE_NODES);
    node_types.resize(MAX_ACTIVE_NODES);
  }

  [[nodiscard]] inline const std::string_view get_scene_type() const noexcept { return scene_type_name; }

  [[nodiscard]] bool is_node_valid(const NodeID node_id) const noexcept;
  const NodeID get_nodes_parent(const NodeID node_id) const noexcept;
  std::vector<NodeID> get_nodes_children(const NodeID node_id) const noexcept;

  [[nodiscard]] NodeID create_node(NodeID parent_id, std::string_view node_type);
  void destroy_node(NodeID node_id);
  void reparent_node(NodeID node_id, NodeID new_parent_id);

  /**
   * @return The highest index part of NodeID of a node that exists.
   */
  [[nodiscard]] inline uint32_t get_highest_allocated_node_index() const noexcept { return next_unused_id.load(); }

  void update();
  void end_frame();

  void clear_nodes();

  Result<void> serialize(nlohmann::json& json) const;
  Result<void> deserialize(const nlohmann::json& json);

  void add_manager(std::unique_ptr<INodeManager> manager);
  void add_node_type(std::string_view node_type, std::span<const std::string> node_managers);

  template <typename T>
  T* get_manager() const {
    auto it = managers_map.find(T::TYPE_ID);
    if (it != managers_map.end()) {
      return static_cast<T*>(it->second.get());
    }
    return nullptr;
  }

 private:
  std::string scene_type_name;

  [[nodiscard]] NodeID generate_id();

  std::atomic<uint32_t> next_unused_id {0};
  std::vector<uint32_t> recycled_ids;
  std::vector<uint32_t> generations; // Index = the raw array slot, Value = the current active generation
  std::vector<Node> nodes;
  std::vector<std::string> node_types;

  std::unordered_map<std::string, std::unique_ptr<INodeManager>, StringHash, std::equal_to<>> managers_map;
  std::vector<INodeManager*> managers_flat_array;

  struct NodeDefinition {
    std::vector<INodeManager*> managers;
  };

  std::unordered_map<std::string, NodeDefinition, StringHash, std::equal_to<>> node_type_definitions;
};

} // mole