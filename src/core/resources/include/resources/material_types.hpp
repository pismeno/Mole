#pragma once

#include <cstdint>
#include <functional>
#include <limits>

namespace mole {

/**
 * @brief Unique identifier for a material. It consists of a 32-bit index and a 32-bit generation-counter.
 */
struct MaterialID {
  uint64_t id;

  /**
   * @brief Invalid MaterialID, used to represent an invalid material.
   */
  static const MaterialID INVALID;

  static constexpr MaterialID from(uint32_t index, uint32_t generation) noexcept {
    return MaterialID{ (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index) };
  }

  constexpr uint32_t index() const noexcept {
    return static_cast<uint32_t>(id);
  }

  constexpr uint32_t generation() const noexcept {
    return static_cast<uint32_t>(id >> 32);
  }

  constexpr bool operator==(const MaterialID& other) const noexcept = default;
};

inline constexpr MaterialID MaterialID::INVALID{ std::numeric_limits<uint64_t>::max() };

} // mole

namespace std {

template <>
struct hash<mole::MaterialID> {
  std::size_t operator()(const mole::MaterialID& m) const noexcept {
    return std::hash<uint64_t>{}(m.id);
  }
};

} // std
