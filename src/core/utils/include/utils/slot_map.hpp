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
  static constexpr uint32_t UNLIMITED = UINT32_MAX;

  SlotMap() = default;

  /** @brief Creates a slot map that never holds more than `max_slots` slots. */
  explicit SlotMap(uint32_t max_slots) : max_slots(max_slots) {}

  /** @brief Stores `value` and returns its ID, or `TID::INVALID` if the slot map is full. */
  TID insert(T&& value) {
    const uint32_t index = acquire_index();
    if (index == UNLIMITED) {
      return TID::INVALID;
    }
    slots[index].emplace(std::move(value));
    ++live_count;
    return TID::from(index, generations[index]);
  }

  /** @brief Stores a copy of `value` and returns its ID, or `TID::INVALID` if the slot map is full. */
  TID insert(const T& value) {
    T copy = value;
    return insert(std::move(copy));
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
    --live_count;
    return value;
  }

  /** @brief Removes every value. All previously issued IDs go stale; the next inserts reuse slots from index 0. */
  void clear() {
    for (uint32_t index = 0; index < slots.size(); ++index) {
      if (slots[index].has_value()) {
        slots[index].reset();
        ++generations[index];
      }
    }
    free_list.clear();
    for (uint32_t index = static_cast<uint32_t>(slots.size()); index > 0; --index) {
      free_list.push_back(index - 1);
    }
    live_count = 0;
  }

  /** @brief Calls `fn(TID, T&)` for every live value, in slot order. */
  template <typename TFn>
  void for_each(TFn&& fn) {
    for (uint32_t index = 0; index < slots.size(); ++index) {
      if (slots[index].has_value()) {
        fn(TID::from(index, generations[index]), *slots[index]);
      }
    }
  }

  /** @brief Calls `fn(TID, const T&)` for every live value, in slot order. */
  template <typename TFn>
  void for_each(TFn&& fn) const {
    for (uint32_t index = 0; index < slots.size(); ++index) {
      if (slots[index].has_value()) {
        fn(TID::from(index, generations[index]), *slots[index]);
      }
    }
  }

  /** @brief Returns the number of live values. */
  [[nodiscard]] uint32_t size() const { return live_count; }

  /** @brief Returns the number of slots ever allocated (high-water mark); every live index is below it. */
  [[nodiscard]] uint32_t slot_count() const { return static_cast<uint32_t>(slots.size()); }

private:
  uint32_t acquire_index() {
    if (!free_list.empty()) {
      const uint32_t index = free_list.back();
      free_list.pop_back();
      return index;
    }
    if (slots.size() >= max_slots) {
      return UNLIMITED;
    }
    const uint32_t index = static_cast<uint32_t>(slots.size());
    slots.emplace_back();
    generations.push_back(0);
    return index;
  }

  uint32_t max_slots{UNLIMITED};
  uint32_t live_count{0};
  std::vector<std::optional<T>> slots;
  std::vector<uint32_t> generations;
  std::vector<uint32_t> free_list;
};

} // mole
