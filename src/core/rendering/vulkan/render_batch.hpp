#pragma once

#include <cstdint>

#include "rendering/vulkan/combined_shader_key.hpp"

namespace mole {

struct RenderBatch {
  CombinedShaderKey shader_key;
  uint32_t first_quad;
  uint32_t quad_count;
  uint32_t first_material_offset;
  uint32_t material_count;
};

} // mole
