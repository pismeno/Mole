#pragma once

#include <concepts>

#include "node_system/inode_manager.hpp"

namespace mole {

template<std::derived_from<INodeManager>... TManagers>
struct ManagerList {};

template <typename T>
struct is_manager_list : std::false_type {};

template <std::derived_from<INodeManager>... TManagers>
struct is_manager_list<ManagerList<TManagers...>> : std::true_type {};

template <typename T>
concept CManagerList = is_manager_list<T>::value;

} // mole