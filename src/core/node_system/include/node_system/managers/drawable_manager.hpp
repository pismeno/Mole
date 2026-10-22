#pragma once

#include "node_system/inode_manager.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include "node_system/node_types.hpp"
#include "resources/material_types.hpp"
#include "utils/result.hpp"

namespace mole {

struct Drawable {
  bool is_visible = false;

  glm::vec2 size = {1.0f, 1.0f};
  MaterialID material_id = MaterialID::INVALID;
  uint32_t z_index = 0;
};

class DrawableManager : public NodeManagerBase<DrawableManager> {
 public:
  explicit DrawableManager();

  static constexpr const char* TYPE_ID = "drawable_manager";

  [[nodiscard]] inline bool has_drawable(NodeID node_id) const noexcept { return drawables[node_id.index()].is_visible; }

  /**
   * Gets the draw information of the given node.
   * @param node_id ID of the node to get the draw information for.
   * @return Drawable struct tied to this node
   */
  [[nodiscard]] inline const Drawable& get_drawable(NodeID node_id) const noexcept { return drawables[node_id.index()]; }

  /**
   * Gets all the draw information of all nodes in the scene.
   * @return reference to the vector of drawables, it is indexed by NodeID.index().
   */
  [[nodiscard]] inline const std::vector<Drawable>& get_all_drawables() const noexcept { return drawables; }

  Result<void> set_node_visible(NodeID node_id, bool visible);
  Result<void> set_node_size(NodeID node_id, glm::vec2 size);
  Result<void> set_node_material(NodeID node_id, MaterialID material_id);
  Result<void> set_node_z_index(NodeID node_id, uint32_t z_index);

  void on_node_destroyed(NodeID node_id) noexcept override;
  void on_node_require_manager(NodeID node_id) noexcept override;
  void on_scene_clear() noexcept override;
  Result<void> on_serialize_scene(nlohmann::json& json) const override;
  Result<void> on_deserialize_scene(const nlohmann::json& json) override;

 private:
  std::vector<Drawable> drawables;
};

} // mole
