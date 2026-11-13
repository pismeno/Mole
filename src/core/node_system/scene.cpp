#include "node_system/scene.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "node_system/inode_manager.hpp"
#include "node_system/node.hpp"
#include "node_system/node_types.hpp"

namespace mole {

Scene::Scene(std::string scene_type_name) : scene_type_name(std::move(scene_type_name)) {
  nodes.resize(MAX_ACTIVE_NODES);
  generations.resize(MAX_ACTIVE_NODES);
  node_types.resize(MAX_ACTIVE_NODES);
  node_uuids.resize(MAX_ACTIVE_NODES);
}

Scene::~Scene() = default;

INodeManager* Scene::get_manager_by_name(std::string_view type_id) const {
  auto it = managers_map.find(type_id);
  if (it != managers_map.end()) {
    return it->second.get();
  }
  return nullptr;
}

[[nodiscard]] NodeID Scene::generate_id() {
  if (!recycled_ids.empty()) {
    uint32_t recycled = recycled_ids.back();
    recycled_ids.pop_back();
    uint32_t current_gen = generations[recycled];

    return NodeID::from(recycled, current_gen);
  }

  if (next_unused_id >= MAX_ACTIVE_NODES) {
    return NodeID::INVALID;
  }

  uint32_t new_id = next_unused_id++;
  generations[new_id] = 0;

  return NodeID::from(new_id, 0);
}

[[nodiscard]] bool Scene::is_node_valid(NodeID node_id) const noexcept {
  if (node_id == NodeID::INVALID || node_id.index() >= MAX_ACTIVE_NODES) {
    return false;
  }
  return generations[node_id.index()] == node_id.generation();
}

[[nodiscard]] const NodeID Scene::get_nodes_parent(NodeID node_id) const noexcept {
  if (!is_node_valid(node_id)) return NodeID::INVALID;

  return nodes[node_id.index()].parent_id;
}

[[nodiscard]] std::vector<NodeID> Scene::get_nodes_children(NodeID node_id) const noexcept {
  if (!is_node_valid(node_id)) return {};

  return nodes[node_id.index()].children_ids;
}

Result<UUID> Scene::get_uuid(NodeID node_id) const {
  if (!is_node_valid(node_id)) {
    return mole::fail("Node ID is not valid");
  }

  return node_uuids[node_id.index()];
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

  NodeID new_id = generate_id();
  if (new_id == NodeID::INVALID) {
    return NodeID::INVALID;
  }

  const uint32_t index = new_id.index();
  nodes[index].id = new_id;
  nodes[index].parent_id = NodeID::INVALID;
  node_types[index] = node_type;
  node_uuids[index] = uuid;
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
  if (!is_node_valid(node_id)) return;

  Node* node = &nodes[node_id.index()];
  if (!node) {
    std::cerr << "Node with provided ID not found";
    return;
  }

  if (node->parent_id != NodeID::INVALID) {
    Node* parent = &nodes[node->parent_id.index()];
    if (parent) {
      auto& siblings = parent->children_ids;
      siblings.erase(std::remove(siblings.begin(), siblings.end(), node_id), siblings.end());
    }
  }

  std::vector<NodeID> children_to_kill = node->children_ids;
  for (NodeID child_id : children_to_kill) {
    destroy_node(child_id);
  }

  for (auto& manager : managers_flat_array) {
    manager->on_node_destroyed(node_id);
  }

  uint32_t idx = node_id.index();

  generations[idx]++;

  recycled_ids.push_back(idx);

  node->id = NodeID::INVALID;
  node->parent_id = NodeID::INVALID;
  node->children_ids.clear();

  UUID dead_uuid = node_uuids[node_id.index()];
  uuid_to_node_id.erase(dead_uuid);
}

void Scene::clear_nodes() {
  for (auto& manager : managers_flat_array) {
    manager->on_scene_clear();
  }

  for (uint32_t i = 0; i < get_highest_allocated_node_index(); ++i) {
    generations[i]++; // Invalidates all existing handles globally
    nodes[i].id = NodeID::INVALID;
    nodes[i].parent_id = NodeID::INVALID;
    nodes[i].children_ids.clear();
  }

  recycled_ids.clear();
  next_unused_id.store(0);
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

  if (node_id == new_parent_id) {
    std::cerr << "Cannot parent a node to itself.";
    return;
  }

  Node* child = &nodes[node_id.index()];
  Node* new_parent = &nodes[new_parent_id.index()];

  if (!child) {
    std::cerr << "Node with provided ID not found";
    return;
  }

  if (!new_parent) {
    std::cerr << "Parent node with provided ID not found";
    return;
  }

  NodeID old_parent_id = child->parent_id;
  if (child->parent_id != NodeID::INVALID) {
    Node* old_parent = &nodes[child->parent_id.index()];
    if (old_parent) {
      auto& siblings = old_parent->children_ids;
      siblings.erase(std::remove(siblings.begin(), siblings.end(), node_id), siblings.end());
    }
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
