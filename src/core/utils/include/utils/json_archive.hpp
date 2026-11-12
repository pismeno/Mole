#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils/archive.hpp"

namespace mole {

class JsonWriteArchive {
 public:
  explicit JsonWriteArchive(nlohmann::json& root) noexcept;

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

class JsonReadArchive {
 public:
  explicit JsonReadArchive(const nlohmann::json& root) noexcept;

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

static_assert(Archive<JsonWriteArchive>);
static_assert(Archive<JsonReadArchive>);

} // mole