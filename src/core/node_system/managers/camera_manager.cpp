#include "node_system/managers/camera_manager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "node_system/node_types.hpp"
#include "resources/view_data.hpp"
#include "utils/result.hpp"

namespace mole {

void CameraManager::on_node_require_manager(NodeID node_id) noexcept {
  cameras[node_id] = Camera{};
}

void CameraManager::on_node_destroyed(NodeID node_id) noexcept {
  cameras.erase(node_id);
}

void CameraManager::on_scene_clear() noexcept {
  cameras.clear();
}

bool CameraManager::has_camera(NodeID node_id) const {
  auto it = cameras.find(node_id);
  return it != cameras.end();
}

Result<void> CameraManager::set_active_camera_node(NodeID node_id) {
  if (!has_camera(node_id)) return mole::fail("Provided node has no Camera");
  active_node = node_id;
  return mole::ok();
}

NodeID CameraManager::get_active_camera_node() const {
  return active_node;
}

Result<void> CameraManager::set_camera(NodeID node_id, const Camera &camera) {
  if (!has_camera(node_id)) return mole::fail("Provided node has no Camera");
  cameras[node_id] = camera;
  return mole::ok();
}

Result<Camera> CameraManager::get_camera(NodeID node_id) const {
  if (!has_camera(node_id)) return mole::fail("Provided node has no Camera");
  return cameras.find(node_id)->second;
}

} // mole