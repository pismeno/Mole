#pragma once

#include <cstdint>
#include <limits>
#include <functional>

namespace mole {

template <typename TTag>
struct ID {
  uint64_t id = std::numeric_limits<uint64_t>::max();

  static const ID INVALID;

  static constexpr ID from(uint32_t index, uint32_t generation) noexcept {
      return ID{ (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index) };
  }

  constexpr uint32_t index() const noexcept {
      return static_cast<uint32_t>(id);
  }

  constexpr uint32_t generation() const noexcept {
      return static_cast<uint32_t>(id >> 32);
  }

  constexpr bool is_valid() const noexcept {
      return id != std::numeric_limits<uint64_t>::max();
  }

  constexpr bool operator==(const ID& other) const noexcept = default;
};

template <typename TTag>
inline constexpr ID<TTag> ID<TTag>::INVALID{ std::numeric_limits<uint64_t>::max() };
  
} // mole

namespace std {
  
template <typename TTag>
struct hash<mole::ID<TTag>> {
  size_t operator()(const mole::ID<TTag>& h) const noexcept {
    return std::hash<uint64_t>{}(h.id);
  }
};

} // std