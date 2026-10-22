#pragma once

#include <vector>

#include "node_system/node_types.hpp"

namespace mole {

class Scene;

class Node {

  friend class Scene;

 public:
  Node() = default;

  inline const NodeID get_id() const noexcept { return id; }
  inline const NodeID get_parent_id() const noexcept { return parent_id; }
  inline const std::vector<NodeID>& get_children_ids() const noexcept { return children_ids; }

 private:
  NodeID id = NodeID::INVALID;
  NodeID parent_id = NodeID::INVALID;
  std::vector<NodeID> children_ids;
};

} // mole