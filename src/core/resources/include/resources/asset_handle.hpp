#pragma once

#include <cstdint>
#include <limits>

#include "utils/uuid.hpp"

namespace mole {

template <typename T> class AssetPool;

template <typename T>
class AssetHandle {
  friend class AssetPool<T>;

 public:
  AssetHandle() = default;
  ~AssetHandle();

  AssetHandle(const AssetHandle& other);
  AssetHandle& operator=(const AssetHandle& other);

  AssetHandle(AssetHandle&& other) noexcept;
  AssetHandle& operator=(AssetHandle&& other) noexcept;

  [[nodiscard]] bool is_valid() const noexcept;
  explicit operator bool() const noexcept { return is_valid(); }

  [[nodiscard]] const T& get() const noexcept;
  const T* operator->() const noexcept { return &get(); }

  bool operator==(const AssetHandle& other) const {
    return index == other.index && generation == other.generation && pool == other.pool;
  }

  [[nodiscard]] inline uint64_t get_packed_id() const noexcept {
    return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index);
  }
  [[nodiscard]] inline uint32_t get_index() const noexcept { return index; }
  [[nodiscard]] inline uint32_t get_generation() const noexcept { return generation; }

  [[nodiscard]] UUID get_uuid() const noexcept;
  [[nodiscard]] uint32_t get_version() const noexcept;

 private:
  AssetHandle(AssetPool<T>* pool, uint32_t index, uint32_t generation) noexcept;

  void add_ref();
  void release_ref();

  AssetPool<T>* pool = nullptr;
  uint32_t index = (std::numeric_limits<uint32_t>::max)();
  uint32_t generation = 0;
};

} // namespace mole