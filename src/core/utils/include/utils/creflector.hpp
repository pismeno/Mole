#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

#include "utils/uuid.hpp"

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

template <CReflector TReflector, typename T>
inline void serialize(TReflector& re, const char* name, T& value);


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

template <CReflector TReflector>
inline void serialize_field(TReflector& re, const char* name, UUID& uuid) {
    if constexpr (TReflector::is_writing()) {
        std::string str = uuid.to_string();
        re.field(name, str);
    } else {
        std::string str;
        re.field(name, str);
        uuid = UUID::from_string(str);
    }
}

template<typename T>
requires std::is_enum_v<T>
inline void serialize_field(CReflector auto& re, const char* name, T& value) {
    using Underlying = std::underlying_type_t<T>;
    using R = std::remove_cvref_t<decltype(re)>;
    
    Underlying raw = static_cast<Underlying>(value);
    re.field(name, raw);

    if constexpr (!R::is_writing()) { // Only write back if reading
        value = static_cast<T>(raw);
    }
}

template<typename T>
inline void serialize_field(CReflector auto& re, const char* name, std::vector<T>& vec) {
    uint32_t count = static_cast<uint32_t>(vec.size());
    re.begin_array(name, count);

    using R = std::remove_cvref_t<decltype(re)>;
    if constexpr (!R::is_writing()) { // Only resize if reading
        vec.resize(count);
    }

    for (uint32_t i = 0; i < count; ++i) {
        mole::serialize(re, nullptr, vec[i]);
    }

    re.end_array();
}


template<typename T, typename TReflector>
concept CSerializable = CReflector<TReflector> && 
requires(T serializable, TReflector& reflector) {
  { serializable.reflect(reflector) } -> std::same_as<void>;
};

// C++20 standard trick for static_assert inside constexpr blocks
template<typename T> inline constexpr bool always_false_v = false;

template <CReflector TReflector, typename T>
inline void serialize(TReflector& re, const char* name, T& value) {
    
    // Is it a primitive natively supported by the Reflector?
    if constexpr (requires { re.field(name, value); }) {
        re.field(name, value);
    }

    // Does it have an intrusive `.reflect()` method?
    else if constexpr (CSerializable<T, TReflector>) {
        if (name) re.begin_object(name);
        value.reflect(re);
        if (name) re.end_object();
    }
    
    // Does it have a free-standing `serialize_field` overload? (GLM, Enums, Vectors)
    else if constexpr (requires { serialize_field(re, name, value); }) {
        serialize_field(re, name, value);
    }
    // Fail: Unrecognized type
    else {
        static_assert(always_false_v<T>, "Type is not serializable! Implement a .reflect() method or a serialize_field() overload.");
    }
}

} // mole