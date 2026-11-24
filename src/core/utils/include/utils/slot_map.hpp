#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace mole {

/**
 * @brief Generational slot storage. IDs are built with `TID::from(index, generation)` and go stale once erased.
 */
template <typename TID, typename T>
class SlotMap {
public:
  /** @brief Stores `value` and returns its ID. */
  TID insert(T&& value) {
    uint32_t index;
    if (!free_list.empty()) {
      index = free_list.back();
      free_list.pop_back();
    } else {
      index = static_cast<uint32_t>(slots.size());
      slots.emplace_back();
      generations.push_back(0);
    }
    slots[index].emplace(std::move(value));
    return TID::from(index, generations[index]);
  }

  /** @brief Returns the value for `id`, or nullptr if the ID is stale or invalid. */
  [[nodiscard]] T* get(TID id) {
    return contains(id) ? &*slots[id.index()] : nullptr;
  }

  /** @brief Returns the value for `id`, or nullptr if the ID is stale or invalid. */
  [[nodiscard]] const T* get(TID id) const {
    return contains(id) ? &*slots[id.index()] : nullptr;
  }

  /** @brief Returns whether `id` refers to a live value. */
  [[nodiscard]] bool contains(TID id) const {
    if (!id.is_valid()) {
      return false;
    }
    const uint32_t index = id.index();
    return index < slots.size() && slots[index].has_value() && generations[index] == id.generation();
  }

  /** @brief Removes the value for `id` and moves it out, or returns nullopt if the ID is stale or invalid. */
  std::optional<T> erase(TID id) {
    if (!contains(id)) {
      return std::nullopt;
    }
    const uint32_t index = id.index();
    std::optional<T> value = std::move(slots[index]);
    slots[index].reset();
    ++generations[index];
    free_list.push_back(index);
    return value;
  }

private:
  std::vector<std::optional<T>> slots;
  std::vector<uint32_t> generations;
  std::vector<uint32_t> free_list;
};

} // mole
