#pragma once

#include <concepts>

#include "node_system/inode_resolver.hpp"
#include "node_system/node_types.hpp"
#include "utils/creflector.hpp"

namespace mole {

template<typename T, typename TReflector>
concept CManagerSerializable = CReflector<TReflector> && 
requires(T serializable, NodeID id, TReflector& reflector, INodeResolver& node_resolver) {
  { T::TYPE_ID } -> std::convertible_to<std::string_view>;
  { serializable.reflect(id, reflector, node_resolver) } -> std::same_as<void>;
};

} // mole