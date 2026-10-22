#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "rendering/material_shader_layout.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

class MaterialShaderLayoutReflector {
 public:
  explicit MaterialShaderLayoutReflector() = default;
  ~MaterialShaderLayoutReflector() = default;

  Result<MaterialShaderLayout> reflect(const uint32_t* spirv_words, size_t size_bytes) const;

  template <typename A>
  requires (std::derived_from<A, ShaderAsset>)
  inline Result<MaterialShaderLayout> reflect(const AssetHandle<A>& shader) const {
    return reflect(shader->as_32bit_words(), shader->get_size_bytes());
  }
};

} // mole
