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
  using ReflectFn = void (*)(INodeManager* mgr, NodeID node_id, Re& ar);
  using HasNodeFn = bool (*)(const INodeManager* mgr, NodeID node_id);

  struct Entry {
    const char* type_id = nullptr;
    HasNodeFn has_node = nullptr;
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

  template<SerializableManager T>
  void bind() {
    Entry entry{
      T::TYPE_ID,
      [](const INodeManager* base, NodeID node_id) -> bool {
        return static_cast<const T*>(base)->has_node(node_id);
      },
      [](INodeManager* base, NodeID node_id, Re& ar) {
        auto* self = static_cast<T*>(base);
        if constexpr (!Re::is_writing()) {
          self->on_node_require_manager(node_id);
        }
        self->reflect(node_id, ar);
      }
    };
    map_entries[T::TYPE_ID] = entry;
  }
};

} // mole