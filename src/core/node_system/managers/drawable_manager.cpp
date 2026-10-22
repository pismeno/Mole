#include "node_system/managers/drawable_manager.hpp"

#include <cstdint>

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include "node_system/node_types.hpp"
#include "resources/material_types.hpp"
#include "utils/result.hpp"

namespace mole {

DrawableManager::DrawableManager() {
  drawables.resize(MAX_ACTIVE_NODES, Drawable{});
}

void DrawableManager::on_node_destroyed(NodeID node_id) noexcept {
  drawables[node_id.index()].is_visible = false;
}

void DrawableManager::on_node_require_manager(NodeID node_id) noexcept {
  drawables[node_id.index()].is_visible = true;
}

void DrawableManager::on_scene_clear() noexcept {
  for (auto &drawable: drawables) {
    drawable.is_visible = false;
  }
}

Result<void> DrawableManager::on_serialize_scene(nlohmann::json &json) const {
  json["nodes"] = nlohmann::json::array();

  uint32_t node_idx = 0;

  for (node_idx = 0; node_idx < drawables.size(); ++node_idx) {
    if (!drawables[node_idx].is_visible) {
      continue;
    }

    nlohmann::json drawable_json;

    drawable_json["id"] = node_idx;
    drawable_json["z_index"] = drawables[node_idx].z_index;

    json["nodes"].push_back(drawable_json);
  }

  return mole::ok();
}

Result<void> DrawableManager::on_deserialize_scene(const nlohmann::json &json) {
  if (!json.contains("nodes") || !json["nodes"].is_array()) {
    return mole::ok();
  }

  for (const auto& node_json : json["nodes"]) {
    if (!node_json.contains("id") || !node_json["id"].is_number_integer()) {
      continue;
    }

    uint32_t node_idx = node_json["id"].get<uint32_t>();
    drawables[node_idx].is_visible = true;

    if (node_json.contains("z_index") && node_json["z_index"].is_number_integer()) {
      drawables[node_idx].z_index = node_json["z_index"].get<uint32_t>();
    }
  }

  return mole::ok();
}

Result<void> DrawableManager::set_node_visible(NodeID node_id, bool visible) {
  if (node_id.index() >= MAX_ACTIVE_NODES) {
    return mole::fail("Node ID is out of bounds!");
  }

  drawables[node_id.index()].is_visible = visible;
  return mole::ok();
}

Result<void> DrawableManager::set_node_size(NodeID node_id, glm::vec2 size) {
  if (node_id.index() >= MAX_ACTIVE_NODES) {
    return mole::fail("Node ID is out of bounds!");
  }

  drawables[node_id.index()].size = size;
  return mole::ok();
}

Result<void> DrawableManager::set_node_material(NodeID node_id, MaterialID material_id) {
  if (node_id.index() >= MAX_ACTIVE_NODES) {
    return mole::fail("Node ID is out of bounds!");
  }

  drawables[node_id.index()].material_id = material_id;
  return mole::ok();
}

Result<void> DrawableManager::set_node_z_index(NodeID node_id, uint32_t z_index) {
  if (node_id.index() >= MAX_ACTIVE_NODES) {
    return mole::fail("Node ID is out of bounds!");
  }

  drawables[node_id.index()].z_index = z_index;
  return mole::ok();
}

} // mole
