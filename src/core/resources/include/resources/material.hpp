#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/string_hash.hpp"

namespace mole {

using MaterialProperty = std::variant<
    float,
    int,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    AssetHandle<ImageAsset>
    >;

struct Material {
  std::unordered_map<std::string, MaterialProperty, StringHash, std::equal_to<>> properties;

  AssetHandle<VertexShaderAsset> vertex_shader;
  AssetHandle<FragmentShaderAsset> fragment_shader;
};

} // mole
