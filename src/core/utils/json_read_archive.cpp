#include "utils/json_archive.hpp"

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace mole {

JsonReadArchive::JsonReadArchive(const nlohmann::json& root) noexcept {
  scope_stack.push_back({&root, 0});
}

template<typename T>
void JsonReadArchive::read_primitive(const char* name, T& value) {
  Scope& curr = current();
  if (!curr.node) return;

  if (curr.node->is_array()) {
    if (curr.array_index < curr.node->size()) {
      const auto& elem = (*curr.node)[curr.array_index++];
      if (!elem.is_null()) {
        value = elem.get<T>();
      }
    }
  } else if (name != nullptr && curr.node->is_object()) {
    auto it = curr.node->find(name);
    if (it != curr.node->end() && !it->is_null()) {
      value = it->get<T>();
    }
  }
}

void JsonReadArchive::field(const char* name, int32_t& value) { read_primitive(name, value); }
void JsonReadArchive::field(const char* name, uint32_t& value) { read_primitive(name, value); }
void JsonReadArchive::field(const char* name, uint64_t& value) { read_primitive(name, value); }
void JsonReadArchive::field(const char* name, float& value) { read_primitive(name, value); }
void JsonReadArchive::field(const char* name, bool& value) { read_primitive(name, value); }
void JsonReadArchive::field(const char* name, std::string& value) { read_primitive(name, value); }

void JsonReadArchive::begin_object(const char* name) {
  static const nlohmann::json empty_object = nlohmann::json::object();
  Scope& curr = current();

  if (curr.node && curr.node->is_array()) {
    if (curr.array_index < curr.node->size() && (*curr.node)[curr.array_index].is_object()) {
      scope_stack.push_back({&(*curr.node)[curr.array_index++], 0});
      return;
    }
    ++curr.array_index;
  } else if (name != nullptr && curr.node && curr.node->is_object()) {
    auto it = curr.node->find(name);
    if (it != curr.node->end() && it->is_object()) {
      scope_stack.push_back({&(*it), 0});
      return;
    }
  }

  scope_stack.push_back({&empty_object, 0});
}

void JsonReadArchive::end_object() {
  if (scope_stack.size() > 1) {
    scope_stack.pop_back();
  }
}

void JsonReadArchive::begin_array(const char* name, uint32_t& count) {
  static const nlohmann::json empty_array = nlohmann::json::array();
  Scope& curr = current();

  if (curr.node && curr.node->is_array()) {
    if (curr.array_index < curr.node->size() && (*curr.node)[curr.array_index].is_array()) {
      const auto& arr = (*curr.node)[curr.array_index++];
      count = static_cast<uint32_t>(arr.size());
      scope_stack.push_back({&arr, 0});
      return;
    }
    ++curr.array_index;
  } else if (name != nullptr && curr.node && curr.node->is_object()) {
    auto it = curr.node->find(name);
    if (it != curr.node->end() && it->is_array()) {
      count = static_cast<uint32_t>(it->size());
      scope_stack.push_back({&(*it), 0});
      return;
    }
  }

  count = 0;
  scope_stack.push_back({&empty_array, 0});
}

void JsonReadArchive::end_array() {
  if (scope_stack.size() > 1) {
    scope_stack.pop_back();
  }
}

} // mole