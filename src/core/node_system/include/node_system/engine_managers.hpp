#pragma once

#include "node_system/manager_list.hpp"
#include "node_system/managers/camera_manager.hpp"
#include "node_system/managers/drawable_manager.hpp"
#include "node_system/managers/transform_manager.hpp"

namespace mole {

using EngineManagers = ManagerList<
  TransformManager, 
  DrawableManager, 
  CameraManager
>;
  
} // mole