#include "node_system/render_gatherer.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>

#include "node_system/managers/drawable_manager.hpp"
#include "node_system/managers/transform_manager.hpp"
#include "node_system/scene.hpp"
#include "resources/render_commands.hpp"

namespace mole {

std::vector<RenderQuadCommand> RenderGatherer::gather_render_commands(Scene& scene) {
  auto drawable_manager = scene.get_manager<DrawableManager>();
  auto transform_manager = scene.get_manager<TransformManager>();

  if (!drawable_manager || !transform_manager) return {};

  const auto& matrices = transform_manager->get_all_matrices();
  const auto& drawables = drawable_manager->get_all_drawables();

  uint32_t max_index = scene.get_highest_allocated_node_index();

  std::vector<RenderQuadCommand> commands;
  commands.reserve(max_index);

  for (uint32_t i = 0; i < max_index; ++i) {
    if (drawables[i].is_visible) {
      glm::mat3 size_offset(1.0f);
      size_offset[0][0] = drawables[i].size.x;
      size_offset[1][1] = drawables[i].size.y;

      glm::mat3 final_transform = matrices[i] * size_offset;

      commands.emplace_back(RenderQuadCommand{
          .transform = final_transform,
          .material_id = drawables[i].material_id,
          .z_index = drawables[i].z_index
      });
    }
  }

  return commands;
}

} // mole
