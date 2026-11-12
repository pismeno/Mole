#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "node_system/inode_manager.hpp"
#include "node_system/node.hpp"
#include "node_system/node_types.hpp"
#include "node_system/manager_reflector_binder.hpp"
#include "utils/reflector.hpp"
#include "utils/string_hash.hpp"

namespace mole {

class Scene {

  friend class NodeHandle;

 public:
  explicit Scene(std::string scene_type_name);
  ~Scene();

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

  void add_manager(std::unique_ptr<INodeManager> manager);
  void add_node_type(std::string_view node_type, std::span<const std::string> node_managers);

  INodeManager* get_manager_by_name(std::string_view type_id) const;

  template <typename T>
  T* get_manager() const {
    return static_cast<T*>(get_manager_by_name(T::TYPE_ID));
  }

  template <Reflector Re>
  void reflect(Re& re, const ManagerReflectorBinder<Re>& binder) {
    if constexpr (!Re::is_writing()) {
      clear_nodes();
    }

    std::vector<NodeID> active_nodes;
    if constexpr (Re::is_writing()) {
      for (uint32_t i = 0; i < get_highest_allocated_node_index(); ++i) {
        NodeID id = NodeID::from(i, generations[i]);
        if (is_node_valid(id)) {
          active_nodes.push_back(id);
        }
      }
    }

    uint32_t node_count = Re::is_writing() ? static_cast<uint32_t>(active_nodes.size()) : 0;
    re.begin_array("nodes", node_count);

    struct PendingParent { NodeID child; uint64_t saved_parent_id; };
    std::vector<PendingParent> pending_parents;
    std::unordered_map<uint64_t, NodeID> id_remapping_table;

    for (uint32_t i = 0; i < node_count; ++i) {
      re.begin_object(nullptr);

      uint64_t saved_id = 0;
      uint64_t saved_parent_id = 0;
      std::string node_type_name_str;

      if constexpr (Re::is_writing()) {
        NodeID current = active_nodes[i];
        saved_id = current.id; 
        
        NodeID parent = get_nodes_parent(current);
        saved_parent_id = is_node_valid(parent) ? parent.id : 0; 
        node_type_name_str = node_types[current.index()];
      }

      re.field("id", saved_id);
      re.field("parent_id", saved_parent_id);
      re.field("node_type", node_type_name_str);

      NodeID runtime_node;
      if constexpr (!Re::is_writing()) {
        runtime_node = create_node(NodeID::INVALID, node_type_name_str); 
        
        id_remapping_table[saved_id] = runtime_node;

        if (saved_parent_id != 0) {
          pending_parents.push_back({runtime_node, saved_parent_id});
        }
      } else {
        runtime_node = active_nodes[i];
      }

      re.begin_object("managers");
      
      auto def_it = node_type_definitions.find(node_type_name_str);
      if (def_it != node_type_definitions.end()) {
        
        for (INodeManager* mgr : def_it->second.managers) {
          if (const auto* entry = binder.get_entry(mgr->get_name())) {
            
            re.begin_object(entry->type_id);
            entry->reflect(mgr, runtime_node, re);
            re.end_object();
            
          }
        }
      }
      
      re.end_object(); // components
      re.end_object(); // node
    }
    
    re.end_array(); // nodes

    if constexpr (!Re::is_writing()) {
      for (const auto& pending : pending_parents) {
        auto it = id_remapping_table.find(pending.saved_parent_id);
        if (it != id_remapping_table.end()) {
          reparent_node(pending.child, it->second);
        }
      }
    }
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
