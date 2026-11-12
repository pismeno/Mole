#pragma once

#include "node_system/inode_manager.hpp"

#include <unordered_map>

#include <glm/vec4.hpp>

#include "node_system/inode_resolver.hpp"
#include "node_system/node_types.hpp"
#include "utils/result.hpp"
#include "utils/creflector.hpp"

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

class CameraManager : public NodeManager<CameraManager> {
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

  template<CReflector TReflector>
  void reflect(NodeID node_id, TReflector& re, INodeResolver& node_resolver) {
    auto it = cameras.find(node_id);
    if (it == cameras.end()) {
      if constexpr (TReflector::is_writing()) {
        cameras[node_id] = Camera{};
        it = cameras.find(node_id);
      } else {
        return;
      }
    }
  
    Camera& camera = it->second;
  
    re.field("size", camera.size);
    re.field("aspect_ratio", camera.aspect_ratio);
    re.field("near_plane", camera.near_plane);
    re.field("far_plane", camera.far_plane);
    re.field("zoom", camera.zoom);
    serialize_field(re, "viewport", camera.viewport);
  }
private:
  NodeID active_node = NodeID::INVALID;

  std::unordered_map<NodeID, Camera> cameras;
};

} // mole