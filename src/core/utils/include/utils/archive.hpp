#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace mole {

template<typename T>
concept Archive = requires(T ar, const char* name, int32_t& i32, uint32_t& u32, uint64_t& u64, float& f, bool& b, std::string& s) {
  { T::is_writing() } -> std::same_as<bool>;

  // Core Primitives
  { ar.field(name, i32) } -> std::same_as<void>;
  { ar.field(name, u32) } -> std::same_as<void>;
  { ar.field(name, u64) } -> std::same_as<void>;
  { ar.field(name, f) } -> std::same_as<void>;
  { ar.field(name, b) } -> std::same_as<void>;
  { ar.field(name, s) } -> std::same_as<void>;

  // Structural Boundaries
  { ar.begin_object(name) } -> std::same_as<void>;
  { ar.end_object() } -> std::same_as<void>;
  { ar.begin_array(name, u32) } -> std::same_as<void>;
  { ar.end_array() } -> std::same_as<void>;
};

inline void serialize_field(Archive auto& ar, const char* name, glm::vec2& v) {
  ar.begin_object(name);
  ar.field("x", v.x);
  ar.field("y", v.y);
  ar.end_object();
}

inline void serialize_field(Archive auto& ar, const char* name, glm::vec3& v) {
  ar.begin_object(name);
  ar.field("x", v.x);
  ar.field("y", v.y);
  ar.field("z", v.z);
  ar.end_object();
}

template<typename T>
requires std::is_enum_v<T>
inline void serialize_field(Archive auto& ar, const char* name, T& value) {
  using Underlying = std::underlying_type_t<T>;
  Underlying raw = static_cast<Underlying>(value);
  ar.field(name, raw);

  if constexpr (!std::remove_reference_t<decltype(ar)>::is_writing) {
    value = static_cast<T>(raw);
  }
}

template<typename T>
inline void serialize_field(Archive auto& ar, const char* name, std::vector<T>& vec) {
  uint32_t count = static_cast<uint32_t>(vec.size());
  ar.begin_array(name, count);

  if constexpr (!std::remove_reference_t<decltype(ar)>::is_writing) {
    vec.resize(count);
  }

  for (uint32_t i = 0; i < count; ++i) {
    if constexpr (requires { vec[i].reflect(ar); }) {
      ar.begin_object(nullptr);
      vec[i].reflect(ar);
      ar.end_object();
    } else if constexpr (requires { serialize_field(ar, nullptr, vec[i]); }) {
      serialize_field(ar, nullptr, vec[i]);
    } else {
      ar.field(nullptr, vec[i]);
    }
  }

  ar.end_array();
}

} // mole