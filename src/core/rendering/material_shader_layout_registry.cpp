#include "rendering/material_shader_layout_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "rendering/material_shader_layout.hpp"
#include "rendering/material_shader_layout_reflector.hpp"
#include "rendering/rendering_context.hpp"
#include "utils/result.hpp"

namespace mole {

MaterialShaderLayoutRegistry::MaterialShaderLayoutRegistry(const RenderingContext& context)
    : material_shader_layout_reflector(context.material_shader_layout_reflector) {}

MaterialShaderLayoutRegistry::~MaterialShaderLayoutRegistry() = default;

Result<MaterialShaderLayout> MaterialShaderLayoutRegistry::get(
    uint64_t shader_id, const uint32_t* spirv_words, size_t size_bytes) {
  auto it = material_shader_layouts.find(shader_id);
  if (it != material_shader_layouts.end()) {
    return it->second;
  }

  auto r_reflect = material_shader_layout_reflector->reflect(spirv_words, size_bytes);
  if (!r_reflect) return r_reflect;

  material_shader_layouts[shader_id] = std::move(r_reflect).value();
  return material_shader_layouts[shader_id];
}

} // mole

