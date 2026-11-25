#include "node_system/scene.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "node_system/inode_manager.hpp"
#include "node_system/node.hpp"
#include "node_system/node_types.hpp"

namespace mole {

Scene::Scene(std::string scene_type_name) :
  scene_type_name(std::move(scene_type_name)) {}

Scene::~Scene() = default;

INodeManager* Scene::get_manager_by_name(std::string_view type_id) const {
  auto it = managers_map.find(type_id);
  if (it != managers_map.end()) {
    return it->second.get();
  }
  return nullptr;
}

[[nodiscard]] bool Scene::is_node_valid(NodeID node_id) const noexcept {
  return nodes.contains(node_id);
}

[[nodiscard]] const NodeID Scene::get_nodes_parent(NodeID node_id) const noexcept {
  const NodeRecord* record = nodes.get(node_id);
  return record ? record->node.parent_id : NodeID::INVALID;
}

[[nodiscard]] std::vector<NodeID> Scene::get_nodes_children(NodeID node_id) const noexcept {
  const NodeRecord* record = nodes.get(node_id);
  return record ? record->node.children_ids : std::vector<NodeID>{};
}

Result<UUID> Scene::get_uuid(NodeID node_id) const {
  const NodeRecord* record = nodes.get(node_id);
  if (!record) {
    return mole::fail("Node ID is not valid");
  }
  return record->uuid;
}

NodeID Scene::get_node_by_uuid(const UUID uuid) const {
  auto it = uuid_to_node_id.find(uuid);
  if (it != uuid_to_node_id.end()) {
    if (is_node_valid(it->second)) {
      return it->second;
    }
  }

  return NodeID::INVALID;
}

[[nodiscard]] NodeID Scene::create_node(NodeID parent_id, std::string_view node_type) {
  return create_node(parent_id, node_type, UUID::generate_v4());
}

[[nodiscard]] NodeID Scene::create_node(NodeID parent_id, std::string_view node_type, UUID uuid) {
  if (uuid.is_nil() || uuid_to_node_id.contains(uuid)) {
    return NodeID::INVALID;
  }

  NodeID new_id = nodes.insert(NodeRecord{
    .type = std::string(node_type),
    .uuid = uuid
  });
  if (new_id == NodeID::INVALID) {
    return NodeID::INVALID;
  }
  NodeRecord& record = *nodes.get(new_id);
  record.node.id = new_id;
  record.node.parent_id = NodeID::INVALID;
  uuid_to_node_id[uuid] = new_id;

  for (auto& manager : managers_flat_array) {
    manager->on_node_created(new_id);
  }

  auto type_it = node_type_definitions.find(node_type);
  if (type_it != node_type_definitions.end()) {
    for (const auto& manager : type_it->second.managers) {
      manager->on_node_require_manager(new_id);
    }
  }

  if (parent_id != NodeID::INVALID && parent_id != new_id) {
    reparent_node(new_id, parent_id);
  }

  return new_id;
}

void Scene::destroy_node(NodeID node_id) {
  NodeRecord* record = nodes.get(node_id);
  if (!record) return;

  const NodeID parent_id = record->node.parent_id;
  if (NodeRecord* parent = nodes.get(parent_id)) {
    auto& siblings = parent->node.children_ids;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), node_id), siblings.end());
  }

  std::vector<NodeID> children_to_kill = record->node.children_ids;
  for (NodeID child_id : children_to_kill) {
    destroy_node(child_id);
  }

  for (auto& manager : managers_flat_array) {
    manager->on_node_destroyed(node_id);
  }

  std::optional<NodeRecord> dead = nodes.erase(node_id);
  if (dead) {
    uuid_to_node_id.erase(dead->uuid);
  }
}

void Scene::clear_nodes() {
  for (auto& manager : managers_flat_array) {
    manager->on_scene_clear();
  }

  nodes.clear();
  uuid_to_node_id.clear();
}

void Scene::reparent_node(NodeID node_id, NodeID new_parent_id) {
  if (node_id == new_parent_id) {
    std::cerr << "Cannot parent a node to itself.\n";
    return;
  }

  if (!is_node_valid(node_id)) {
    std::cerr << "Cannot reparent: Child node is invalid or already destroyed.\n";
    return;
  }

  if (!is_node_valid(new_parent_id)) {
    std::cerr << "Cannot reparent: Target parent node is invalid or already destroyed.\n";
    return;
  }

  Node* child = &nodes.get(node_id)->node;
  Node* new_parent = &nodes.get(new_parent_id)->node;

  NodeID old_parent_id = child->parent_id;
  if (NodeRecord* old_parent = nodes.get(old_parent_id)) {
    auto& siblings = old_parent->node.children_ids;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), node_id), siblings.end());
  }

  child->parent_id = new_parent_id;
  new_parent->children_ids.push_back(node_id);

  for (auto& manager : managers_flat_array) {
    manager->on_node_reparented(node_id, new_parent_id, old_parent_id);
  }
}

void Scene::end_frame() {
  for (auto& manager : managers_flat_array) {
    manager->on_frame_end();
  }
}

void Scene::update() {
  for (auto& manager : managers_flat_array) {
    manager->on_update();
  }
}

void Scene::add_manager(std::unique_ptr<INodeManager> manager) {
  std::string manager_name = manager->get_name();
  if (managers_map.find(manager_name) != managers_map.end()) {
    std::cerr << "A manager with the name '" << manager_name << "' is already registered";
    return;
  }

  managers_flat_array.push_back(manager.get());
  managers_map[manager_name] = std::move(manager);
}

void Scene::add_node_type(std::string_view node_type, std::span<const std::string> node_managers) {
  if (node_type_definitions.find(node_type) != node_type_definitions.end()) {
    std::cerr << "A node type with the name '" << std::string(node_type) << "' is already registered";
    return;
  }

  NodeDefinition node_definition;

  for (auto& manager_name : node_managers) {
    auto manager_it = managers_map.find(manager_name);
    if (manager_it == managers_map.end()) {
      std::cerr << "Node type '" << std::string(node_type) << "' requires manager '" << std::string(manager_name)
                << "' which is not registered";
      return;
    }

    node_definition.managers.push_back(manager_it->second.get());
  }

  node_type_definitions.emplace(node_type, std::move(node_definition));
}

void Scene::set_source_asset(AssetHandle<SceneAsset> asset) { source_scene_asset = std::move(asset); }

AssetHandle<SceneAsset> Scene::get_source_asset() const { return source_scene_asset; }

} // mole
