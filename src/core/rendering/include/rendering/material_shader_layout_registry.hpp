#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "rendering/material_shader_layout.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

class MaterialShaderLayoutReflector;
struct RenderingContext;

class MaterialShaderLayoutRegistry {
 public:
  explicit MaterialShaderLayoutRegistry(const RenderingContext& context);
  ~MaterialShaderLayoutRegistry();

  Result<MaterialShaderLayout> get(uint64_t shader_id, const uint32_t* spirv_words, size_t size_bytes);

  template <typename A>
  requires (std::derived_from<A, ShaderAsset>)
  Result<MaterialShaderLayout> get(const AssetHandle<A>& shader) {
    return get(shader.get_packed_id(), shader->as_32bit_words(), shader->get_size_bytes());
  }

 private:
  std::unordered_map<uint64_t, MaterialShaderLayout> material_shader_layouts;
  MaterialShaderLayoutReflector* material_shader_layout_reflector = nullptr;
};

} // mole
