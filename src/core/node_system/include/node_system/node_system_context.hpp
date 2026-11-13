#pragma once

#include "node_system/engine_managers.hpp"
#include "node_system/manager_reflector_binder_factory.hpp"

namespace mole {

struct ResourcesContext;

class SceneFactory;
class RenderGatherer;

/**
 * @brief Context for the node system layer.
 */
struct NodeSystemContext {
  ResourcesContext* resources_context;

  SceneFactory* scene_factory;
  RenderGatherer* render_gatherer;
  ManagerReflectorBinderFactory<EngineManagers>* manager_reflector_binder_factory;
};

} // mole