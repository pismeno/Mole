#pragma once

#include "rendering/material_shader_layout.hpp"
#include "rendering/packed_material.hpp"
#include "resources/material.hpp"

namespace mole {

class BindlessTextureRegistry;
struct RenderingContext;

class MaterialPacker {
 public:
  explicit MaterialPacker(RenderingContext& rendering_context);
  ~MaterialPacker() = default;

  PackedMaterial pack(const Material& material, const MaterialShaderLayout& layout) const;
 private:
  BindlessTextureRegistry* bindless_texture_registry;
};

} // mole
