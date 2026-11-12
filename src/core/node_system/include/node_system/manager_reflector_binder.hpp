#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "node_system/inode_manager.hpp"
#include "node_system/manager_list.hpp"
#include "node_system/node_types.hpp"
#include "utils/reflector.hpp"

namespace mole {

template<Reflector Re>
class ManagerReflectorBinder {
 public:
  using ReflectFn = void (*)(INodeManager* mgr, NodeID node_id, Re& re);

  struct Entry {
    const char* type_id = nullptr;
    ReflectFn reflect = nullptr;
  };

  [[nodiscard]] const Entry* get_entry(std::string_view type_id) const noexcept {
    auto it = map_entries.find(std::string(type_id)); 
    if (it != map_entries.end()) {
      return &it->second;
    }
    return nullptr;
  }

  template<typename... Managers>
  [[nodiscard]] static ManagerReflectorBinder<Re> create(ManagerList<Managers...>) {
    ManagerReflectorBinder<Re> binder;
    (binder.template bind<Managers>(), ...);
    return binder;
  }

 private:
  std::unordered_map<std::string, Entry> map_entries;

  template<typename T>
  requires std::derived_from<T, INodeManager>
  void bind() {
    map_entries[T::TYPE_ID] = Entry{
      T::TYPE_ID,
      [](INodeManager* base, NodeID node_id, Re& re) {
        static_cast<T*>(base)->reflect(node_id, re);
      }
    };
  }
};

} // mole