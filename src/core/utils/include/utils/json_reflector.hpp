#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils/creflector.hpp"

namespace mole {

class JsonWriteReflector {
 public:
  explicit JsonWriteReflector(nlohmann::json& root) noexcept;

  static constexpr bool is_writing() noexcept { return true; }
  
  void field(const char* name, int32_t& value);
  void field(const char* name, uint32_t& value);
  void field(const char* name, uint64_t& value);
  void field(const char* name, float& value);
  void field(const char* name, bool& value);
  void field(const char* name, std::string& value);

  void begin_object(const char* name);
  void end_object();
  void begin_array(const char* name, uint32_t& count);
  void end_array();

 private:
  template<typename T>
  void write_primitive(const char* name, const T& value);

  [[nodiscard]] nlohmann::json& current() noexcept { return *node_stack.back(); }

  std::vector<nlohmann::json*> node_stack;
};

class JsonReadReflector {
 public:
  explicit JsonReadReflector(const nlohmann::json& root) noexcept;

  static constexpr bool is_writing() noexcept { return false; }
  
  void field(const char* name, int32_t& value);
  void field(const char* name, uint32_t& value);
  void field(const char* name, uint64_t& value);
  void field(const char* name, float& value);
  void field(const char* name, bool& value);
  void field(const char* name, std::string& value);

  void begin_object(const char* name);
  void end_object();
  void begin_array(const char* name, uint32_t& count);
  void end_array();

 private:
  template<typename T>
  void read_primitive(const char* name, T& value);

  struct Scope {
    const nlohmann::json* node = nullptr;
    size_t array_index = 0;
  };

  [[nodiscard]] Scope& current() noexcept { return scope_stack.back(); }
  [[nodiscard]] const Scope& current() const noexcept { return scope_stack.back(); }

  std::vector<Scope> scope_stack;
};

static_assert(CReflector<JsonWriteReflector>);
static_assert(CReflector<JsonReadReflector>);

} // mole