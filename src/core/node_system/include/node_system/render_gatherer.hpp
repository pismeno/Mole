#pragma once

#include <vector>

#include "resources/render_commands.hpp"

namespace mole {

class Scene;

class RenderGatherer {
 public:
  explicit RenderGatherer() = default;

  /**
   * @brief Generates render commands for the given scene.
   * @param scene reference to the scene
   * @return vector of generated render commands
   */
  std::vector<RenderQuadCommand> gather_render_commands(Scene& scene);
};

} // mole
