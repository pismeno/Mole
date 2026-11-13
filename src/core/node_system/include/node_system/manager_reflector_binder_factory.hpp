// manager_reflector_binder_factory.hpp
#pragma once

#include <concepts>
#include <memory>

#include "node_system/inode_manager.hpp"
#include "node_system/manager_list.hpp"
#include "node_system/manager_reflector_binder.hpp"
#include "utils/creflector.hpp"

namespace mole {

template <typename TManagerList>
class ManagerReflectorBinderFactory;

template <std::derived_from<INodeManager>... TManagers>
class ManagerReflectorBinderFactory<ManagerList<TManagers...>> {
 public:
  explicit ManagerReflectorBinderFactory() = default;
  explicit ManagerReflectorBinderFactory(ManagerList<TManagers...>) {}

  template <CReflector TReflector>
  [[nodiscard]] static std::unique_ptr<ManagerReflectorBinder<TReflector>> create_binder() {
    return std::make_unique<ManagerReflectorBinder<TReflector>>(ManagerList<TManagers...>{});
  }
};

} // mole