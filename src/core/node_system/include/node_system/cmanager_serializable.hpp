#include <concepts>

#include "node_system/node_types.hpp"
#include "utils/creflector.hpp"

namespace mole {

template<typename T, typename TReflector>
concept CManagerSerializable = CReflector<TReflector> && 
requires(T serializable, NodeID id, TReflector& reflector) {
  { serializable.reflect(id, reflector) } -> std::same_as<void>;
};

} // mole