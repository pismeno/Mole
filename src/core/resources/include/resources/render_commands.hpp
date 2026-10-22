#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "resources/material_types.hpp"

namespace mole {

struct RenderQuadCommand {
  glm::mat3x3 transform;

  MaterialID material_id;
  uint32_t z_index;
};

} // mole
