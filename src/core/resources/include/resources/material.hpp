#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "resources/asset_handle.hpp"
#include "utils/creflector.hpp"
#include "utils/string_hash.hpp"
#include "utils/uuid.hpp"

namespace mole {

struct ImageAsset;
struct VertexShaderAsset;
struct FragmentShaderAsset;

enum class EMaterialPropertyType : uint32_t {
  Float = 0,
  Int = 1,
  Vec2 = 2,
  Vec3 = 3,
  Vec4 = 4,
  Texture = 5,
};
using MaterialPropertyType = EMaterialPropertyType;

using MaterialProperty = std::variant<float, int, glm::vec2, glm::vec3, glm::vec4, AssetHandle<ImageAsset>>;

using MaterialPropertyValue = std::variant<float, int, glm::vec2, glm::vec3, glm::vec4, UUID>;

struct MaterialPropertyRecord {
  std::string name;
  EMaterialPropertyType type = EMaterialPropertyType::Float;
  MaterialPropertyValue value;

  template <CReflector TReflector>
  void reflect(TReflector& re) {
    serialize(re, "name", name);
    serialize(re, "type", type);
    if constexpr (TReflector::is_writing()) {
      std::visit(
          [&](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int>) {
              int32_t i = val;
              serialize(re, "value", i);
            } else {
              auto copy = val;
              serialize(re, "value", copy);
            }
          },
          value);
    } else {
      switch (type) {
        case EMaterialPropertyType::Float: {
          float f = 0.0f;
          serialize(re, "value", f);
          value = f;
          break;
        }
        case EMaterialPropertyType::Int: {
          int32_t i = 0;
          serialize(re, "value", i);
          value = static_cast<int>(i);
          break;
        }
        case EMaterialPropertyType::Vec2: {
          glm::vec2 v{0.0f};
          serialize(re, "value", v);
          value = v;
          break;
        }
        case EMaterialPropertyType::Vec3: {
          glm::vec3 v{0.0f};
          serialize(re, "value", v);
          value = v;
          break;
        }
        case EMaterialPropertyType::Vec4: {
          glm::vec4 v{0.0f};
          serialize(re, "value", v);
          value = v;
          break;
        }
        case EMaterialPropertyType::Texture: {
          UUID u{};
          serialize(re, "value", u);
          value = u;
          break;
        }
      }
    }
  }
};

struct Material {
  std::unordered_map<std::string, MaterialProperty, StringHash, std::equal_to<>> properties;

  AssetHandle<VertexShaderAsset> vertex_shader;
  AssetHandle<FragmentShaderAsset> fragment_shader;
};

} // mole
