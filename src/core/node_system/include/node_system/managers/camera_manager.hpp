#pragma once

#include "node_system/inode_manager.hpp"

#include <unordered_map>

#include <glm/vec4.hpp>

#include "node_system/node_types.hpp"
#include "utils/result.hpp"

namespace mole {

enum class AspectRatioMode {
    Stretch,
    KeepAspect,
    Expand
};

struct Camera {
    // Orthographic bounds / sizing
    float size{10.0f};           // Typically half-height in world units (orthographic size)
    float aspect_ratio{16.0f / 9.0f};
    float near_plane{-100.0f};   // Useful in 2D to sort z-layers
    float far_plane{100.0f};

    // Zoom & Screen mapping
    float zoom{1.0f};
    glm::vec4 viewport{0.0f, 0.0f, 1.0f, 1.0f}; // Normalized [x, y, w, h] on target buffer+
};

class CameraManager : public NodeManagerBase<CameraManager> {
public:
  static constexpr const char* TYPE_ID = "camera_manager";

  explicit CameraManager() = default;
  
  bool has_camera(NodeID node_id) const;

  NodeID get_active_camera_node() const;
  Result<void> set_active_camera_node(NodeID node_id);

  Result<void> set_camera(NodeID node_id, const Camera& camera);
  Result<Camera> get_camera(NodeID node_id) const;
  
  void on_node_destroyed(NodeID node_id) noexcept override;
  void on_node_require_manager(NodeID node_id) noexcept override;
  void on_scene_clear() noexcept override;
private:
  NodeID active_node = NodeID::INVALID;

  std::unordered_map<NodeID, Camera> cameras;
};

} // mole