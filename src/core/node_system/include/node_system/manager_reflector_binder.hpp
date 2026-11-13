#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "node_system/inode_manager.hpp"
#include "node_system/inode_resolver.hpp"
#include "node_system/manager_list.hpp"
#include "node_system/node_types.hpp"
#include "node_system/cmanager_serializable.hpp"
#include "utils/creflector.hpp"

namespace mole {

template<CReflector TReflector>
class ManagerReflectorBinder {
 public:
   using ReflectFn = void (*)(INodeManager* mgr, NodeID node_id, TReflector& re, INodeResolver& resolver);
 
   struct Entry {
     const char* type_id = nullptr;
     ReflectFn reflect = nullptr;
   };
   
   template<typename... TManagers>
   explicit ManagerReflectorBinder(ManagerList<TManagers...>) {
     (bind<TManagers>(), ...);
   }

  [[nodiscard]] const Entry* get_entry(std::string_view type_id) const noexcept {
    auto it = map_entries.find(std::string(type_id)); 
    if (it != map_entries.end()) {
      return &it->second;
    }
    return nullptr;
  }
 private:
  std::unordered_map<std::string, Entry> map_entries;

  template<typename T>
  requires std::derived_from<T, INodeManager> && CManagerSerializable<T, TReflector>
  void bind() {
    map_entries[T::TYPE_ID] = Entry{
      T::TYPE_ID,
      [](INodeManager* base, NodeID node_id, TReflector& re, INodeResolver& resolver) {
        static_cast<T*>(base)->reflect(node_id, re, resolver);
      }
    };
  }
};

} // mole