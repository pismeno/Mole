#pragma once

#include "node_system/inode_manager.hpp"

namespace mole {

template<SerializableManager... Managers>
struct ManagerList {};

} // mole