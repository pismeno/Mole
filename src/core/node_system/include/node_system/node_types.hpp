#pragma once

#include <cstdint>
#include <limits>

namespace mole {

/**
 * @brief Unique identifier for a node. It consists of a 32-bit index and a 32-bit generation-counter.
 */
struct NodeID {
  uint64_t id;

  /**
   * @brief Invalid NodeID, used to represent an invalid node.
   */
  static const NodeID INVALID;

  static constexpr NodeID from(uint32_t index, uint32_t generation) noexcept {
    return NodeID{ (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index) };
  }

  constexpr uint32_t index() const noexcept {
    return static_cast<uint32_t>(id);
  }

  constexpr uint32_t generation() const noexcept {
    return static_cast<uint32_t>(id >> 32);
  }

  constexpr bool operator==(const NodeID& other) const noexcept = default;
};

inline constexpr NodeID NodeID::INVALID{ std::numeric_limits<uint64_t>::max() };

/**
* @brief Maximum number of active nodes, hard-coded for now.
*/
static inline constexpr uint32_t MAX_ACTIVE_NODES = 50000;

} // mole
