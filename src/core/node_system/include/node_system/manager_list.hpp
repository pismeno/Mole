#pragma once

#include "node_system/inode_manager.hpp"

namespace mole {

template<std::derived_from<INodeManager>... Managers>
struct ManagerList {};

} // mole