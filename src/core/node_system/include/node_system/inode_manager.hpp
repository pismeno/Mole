#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "node_system/node_types.hpp"
#include "utils/result.hpp"

namespace mole {

/**
 * @brief Base class for all node managers.
 * @note Define 'static constexpr const char* TYPE_ID' in derived classes for registering them in SceneFactory.
 */
class INodeManager {
 public:
  explicit INodeManager() = default;
  virtual ~INodeManager() noexcept = default;

  [[nodiscard]] virtual std::string get_name() const = 0;

  /**
   * @brief Called when a node is destroyed.
   * @note This is also called when the node doesnt have this manager in its definition's managers list.
   */
  virtual void on_node_destroyed(NodeID node_id) noexcept {};

  /**
   * @brief Called for every node that is created.
   */
  virtual void on_node_created(NodeID node_id) noexcept {};

  /**
   * @brief Called for nodes (when created) that have this manager in their definition's managers list.
   */
  virtual void on_node_require_manager(NodeID node_id) noexcept {};

  /**
   * @brief Called when a node is reparented.
   * @note This is also called when node is first created and has a parent. And is called even for nodes that do not have this manager in their definition's managers list.'
   */
  virtual void on_node_reparented(NodeID node_id, NodeID new_parent_id, NodeID old_parent_id) noexcept {};

  /**
   * @brief Called every frame.
   */
  virtual void on_update() noexcept {};

  /**
   * @brief Called after every frame.
   */
  virtual void on_frame_end() noexcept {};

  /**
   * @brief Called when the scene is cleared (when Scene::clear() is called).
   */
  virtual void on_scene_clear() noexcept {};

  /**
   * @brief Called when the scene is being serialized.
   * @param json Json object assigned to this manager to serialize the manager's data into.
   */
  virtual Result<void> on_serialize_scene(nlohmann::json& json) const { return mole::ok(); };

  /**
   * @brief Called when the scene is being deserialized.
   * @param json This manager's data to deserialize from.
   */
  virtual Result<void> on_deserialize_scene(const nlohmann::json& json) { return mole::ok(); };
};

template <typename Derived>
class NodeManagerBase : public INodeManager {
 public:
  std::string get_name() const noexcept override {
    return std::string(Derived::TYPE_ID);
  }
};

} // mole
