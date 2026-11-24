#pragma once

#include "utils/id.hpp"

namespace mole {

/**
 * @brief Unique identifier for a node. It consists of a 32-bit index and a 32-bit generation-counter.
 */
using NodeID = ID<struct NodeIDTag>;

/**
* @brief Maximum number of active nodes, hard-coded for now.
*/
static inline constexpr uint32_t MAX_ACTIVE_NODES = 50000;

} // mole