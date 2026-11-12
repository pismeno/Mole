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
concept CReflector = requires(T re, const char* name, int32_t& i32, uint32_t& u32, uint64_t& u64, float& f, bool& b, std::string& s) {
  { T::is_writing() } -> std::same_as<bool>;

  // Core Primitives
  { re.field(name, i32) } -> std::same_as<void>;
  { re.field(name, u32) } -> std::same_as<void>;
  { re.field(name, u64) } -> std::same_as<void>;
  { re.field(name, f) } -> std::same_as<void>;
  { re.field(name, b) } -> std::same_as<void>;
  { re.field(name, s) } -> std::same_as<void>;

  // Structural Boundaries
  { re.begin_object(name) } -> std::same_as<void>;
  { re.end_object() } -> std::same_as<void>;
  { re.begin_array(name, u32) } -> std::same_as<void>;
  { re.end_array() } -> std::same_as<void>;
};

inline void serialize_field(CReflector auto& re, const char* name, glm::vec2& v) {
  re.begin_object(name);
  re.field("x", v.x);
  re.field("y", v.y);
  re.end_object();
}

inline void serialize_field(CReflector auto& re, const char* name, glm::vec3& v) {
  re.begin_object(name);
  re.field("x", v.x);
  re.field("y", v.y);
  re.field("z", v.z);
  re.end_object();
}

inline void serialize_field(CReflector auto& re, const char* name, glm::vec4& v) {
  re.begin_object(name);
  re.field("x", v.x);
  re.field("y", v.y);
  re.field("z", v.z);
  re.field("w", v.w);
  re.end_object();
}

template<typename T>
requires std::is_enum_v<T>
inline void serialize_field(CReflector auto& re, const char* name, T& value) {
  using Underlying = std::underlying_type_t<T>;
  Underlying raw = static_cast<Underlying>(value);
  re.field(name, raw);

  if constexpr (T::is_writing()) {
    value = static_cast<T>(raw);
  }
}

template<typename T>
inline void serialize_field(CReflector auto& re, const char* name, std::vector<T>& vec) {
  uint32_t count = static_cast<uint32_t>(vec.size());
  re.begin_array(name, count);

  if constexpr (T::is_writing()) {
    vec.resize(count);
  }

  for (uint32_t i = 0; i < count; ++i) {
    if constexpr (requires { vec[i].reflect(re); }) {
      re.begin_object(nullptr);
      vec[i].reflect(re);
      re.end_object();
    } else if constexpr (requires { serialize_field(re, nullptr, vec[i]); }) {
      serialize_field(re, nullptr, vec[i]);
    } else {
      re.field(nullptr, vec[i]);
    }
  }

  re.end_array();
}

} // mole