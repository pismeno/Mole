#pragma once

#include <node_system/inode_manager.hpp>

#include <vector>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <utils/result.hpp>
#include <node_system/node_types.hpp>

namespace mole {

using TransformIndex = uint32_t;
inline constexpr TransformIndex invalid_transform_index = (std::numeric_limits<uint32_t>::max)();

class TransformManager : public NodeManagerBase<TransformManager> {
 public:
  explicit TransformManager();

  static constexpr const char* TYPE_ID = "transform_manager";

  [[nodiscard]] bool has_transform(NodeID node_id) const noexcept;

  /**
   * @brief Gets the global transformation matrix of the given node, this represents scale, translation, shear and rotation.
   */
  [[nodiscard]] inline const glm::mat3x3& get_global_matrix(NodeID node_id) const noexcept { return global_matrices[node_id.index()]; }

  /**
   * @brief Gets all the global transformation matrices of all nodes in the scene.
   * @return reference to the vector of matrices, it is indexed by NodeID.index().
   */
  [[nodiscard]] inline const std::vector<glm::mat3x3>& get_all_matrices() const noexcept { return global_matrices; }

  Result<void> set_node_position(NodeID node_id, glm::vec2 pos);
  Result<void> set_node_scale(NodeID node_id, glm::vec2 scale);
  Result<void> set_node_shear(NodeID node_id, glm::vec2 shear);
  Result<void> set_node_rotation(NodeID node_id, float rotation);

  /**
   * Sets the parent of the given child node, in this manager, this is needed for mantaining the hierarchy.
   */
  void set_spatial_parent(NodeID child_id, NodeID parent_id) noexcept;
  void remove_from_spatial_parent(NodeID child_id) noexcept;

  void on_update() noexcept override;
  void on_node_destroyed(NodeID node_id) noexcept override;
  void on_frame_end() noexcept override;
  void on_node_require_manager(NodeID node_id) noexcept override;
  void on_node_reparented(NodeID node_id, NodeID new_parent_id, NodeID old_parent_id) noexcept override;
  void on_scene_clear() noexcept override;
  Result<void> on_serialize_scene(nlohmann::json& json) const override;
  Result<void> on_deserialize_scene(const nlohmann::json& json) override;

 private:
  /**
   * @brief private information for nodes
   */
  struct TransformInfo {
    glm::vec2 local_position = {0.0f, 0.0f};
    glm::vec2 local_scale = {1.0f, 1.0f};
    glm::vec2 local_shear = {0.0f, 0.0f};
    float local_rotation = 0.0f;

    uint32_t node_idx = NodeID::INVALID.index();
    uint32_t parent_idx = NodeID::INVALID.index();
    uint32_t first_child_idx = NodeID::INVALID.index();
    uint32_t next_sibling_idx = NodeID::INVALID.index();

    bool is_dirty = true;
  };

  TransformInfo* get_transform(uint32_t node_idx);
  void update_node_hierarchy(NodeID node_id, const glm::mat3& parent_global_mat);
  void mark_spatial_children_dirty(NodeID parent_id);


  void detach_from_old_logical_parent(NodeID node_id, NodeID old_parent_id) noexcept;
  void attach_to_new_logical_parent(NodeID node_id, NodeID new_parent_id) noexcept;
  [[nodiscard]] NodeID find_nearest_spatial_parent(NodeID node_id) const noexcept;
  /// Walks down the logical subtree to bind any descendants with a transform to the new spatial parent
  void relink_spatial_children_to_new_parent(NodeID start_node_id, NodeID new_spatial_parent) noexcept;

  std::vector<uint32_t> logical_node_parents;
  std::vector<std::vector<uint32_t>> logical_children;

  std::vector<TransformInfo> transforms;           // Indexed by NodeID
  std::vector<glm::mat3x3> global_matrices;      // Sorted hierarchically by tree depth
  std::vector<TransformIndex> node_to_transform; // The Lookup Table: index is NodeID, value is TransformIndex
};

} // mole