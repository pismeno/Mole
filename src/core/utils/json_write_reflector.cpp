#include "utils/json_reflector.hpp"

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace mole {

JsonWriteReflector::JsonWriteReflector(nlohmann::json& root) noexcept {
  node_stack.push_back(&root);
}

template<typename T>
void JsonWriteReflector::write_primitive(const char* name, const T& value) {
  nlohmann::json& curr = current();
  if (curr.is_array()) {
    curr.push_back(value);
  } else if (name != nullptr) {
    curr[name] = value;
  }
}

void JsonWriteReflector::field(const char* name, int32_t& value) { write_primitive(name, value); }
void JsonWriteReflector::field(const char* name, uint32_t& value) { write_primitive(name, value); }
void JsonWriteReflector::field(const char* name, uint64_t& value) { write_primitive(name, value); }
void JsonWriteReflector::field(const char* name, float& value) { write_primitive(name, value); }
void JsonWriteReflector::field(const char* name, bool& value) { write_primitive(name, value); }
void JsonWriteReflector::field(const char* name, std::string& value) { write_primitive(name, value); }

void JsonWriteReflector::begin_object(const char* name) {
  nlohmann::json& curr = current();
  if (curr.is_array()) {
    curr.push_back(nlohmann::json::object());
    node_stack.push_back(&curr.back());
  } else if (name != nullptr) {
    curr[name] = nlohmann::json::object();
    node_stack.push_back(&curr[name]);
  }
}

void JsonWriteReflector::end_object() {
  if (node_stack.size() > 1) {
    node_stack.pop_back();
  }
}

void JsonWriteReflector::begin_array(const char* name, uint32_t& /*count*/) {
  nlohmann::json& curr = current();
  if (curr.is_array()) {
    curr.push_back(nlohmann::json::array());
    node_stack.push_back(&curr.back());
  } else if (name != nullptr) {
    curr[name] = nlohmann::json::array();
    node_stack.push_back(&curr[name]);
  }
}

void JsonWriteReflector::end_array() {
  if (node_stack.size() > 1) {
    node_stack.pop_back();
  }
}

} // mole