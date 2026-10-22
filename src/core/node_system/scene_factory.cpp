#include "node_system/scene_factory.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "node_system/node_system_context.hpp"
#include "node_system/scene.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

SceneFactory::~SceneFactory() = default;

void SceneFactory::register_manager_factory(std::string_view name, ManagerFactory factory) {
  std::string s_name(name);
  assert(manager_registry.find(s_name) == manager_registry.end() && "Manager name already registered!");
  manager_registry[s_name] = std::move(factory);
}

Result<std::unique_ptr<SceneFactory>> SceneFactory::create(const NodeSystemContext& context) {
  auto factory = std::unique_ptr<SceneFactory>(new SceneFactory());
  auto r_init = factory->init(context);
  if (!r_init) return mole::fail(r_init.error());
  return std::move(factory);
}

Result<void> SceneFactory::init(const NodeSystemContext& context) {
  asset_manager = context.resources_context->asset_manager;
  file_system = context.resources_context->os_context->file_system;
  return mole::ok();
}

Result<std::unique_ptr<Scene>> SceneFactory::create_scene(const std::string& scene_type) const {
  auto scene_def_it = scene_definitions.find(scene_type);

  if (scene_def_it == scene_definitions.end()) {
    return mole::fail("Scene type '" + scene_type + "' is not registered.");
  }

  auto scene = std::make_unique<Scene>(scene_type);
  SceneDefinition scene_definition = scene_def_it->second;

  for (auto& manager_name : scene_definition.managers) {
    auto manager_factory_it = manager_registry.find(manager_name);

    if (manager_factory_it == manager_registry.end()) {
      return mole::fail("Manager '" + manager_name + "' is not registered.");
    }

    ManagerFactory manager_factory = manager_factory_it->second;
    scene->add_manager(manager_factory());
  }

  for (auto& node_name : scene_definition.allowed_nodes) {
    auto node_def_it = node_definitions.find(node_name);

    if (node_def_it == node_definitions.end()) {
      return mole::fail("Node type '" + node_name + "' is not registered.");
    }

    scene->add_node_type(node_name, node_def_it->second.managers);
  }

  return std::move(scene);
}

Result<void> SceneFactory::load_scene_definitions_from(const std::string_view definitions_source) {
  nlohmann::json data = nlohmann::json::parse(definitions_source, nullptr, false);

  if (data.is_discarded()) {
    return mole::fail("Failed to parse JSON from source.");
  }

  if (!data.is_array()) {
    return mole::fail("Expected an array of scene definitions in source.");
  }

  for (const auto& item : data) {
    if (!item.contains("name")) {
      return mole::fail("Scene definition is missing 'name' property in source.");
    }

    SceneDefinition scene_definition;

    if (item.contains("managers")) {
      if (!item["managers"].is_array()) {
        return mole::fail("Scene definition 'managers' property is not an array in source.");
      }

      for (const auto& manager_name : item["managers"]) {
        if (!manager_name.is_string()) {
          return mole::fail("Node definition 'managers' array contains non-string value in source.");
        }

        if (manager_registry.find(manager_name.get<std::string>()) == manager_registry.end()) {
          return mole::fail("Node definition 'managers' array contains unknown manager '" + manager_name.get<std::string>() + "' in source.");
        }
      }

      scene_definition.managers = item["managers"].get<std::vector<std::string>>();
    }

    if (item.contains("allowed_nodes")) {
      if (!item["allowed_nodes"].is_array()) {
        return mole::fail("Scene definition 'allowed_nodes' property is not an array in source.");
      }

      scene_definition.allowed_nodes = item["allowed_nodes"].get<std::vector<std::string>>();
    }

    scene_definitions.emplace(item["name"].get<std::string>(), std::move(scene_definition));
  }

  return mole::ok();
}

Result<void> SceneFactory::load_node_definitions_from(const std::string_view definitions_source) {
  nlohmann::json data = nlohmann::json::parse(definitions_source, nullptr, false);

  if (data.is_discarded()) {
    return mole::fail("Failed to parse JSON from source.");
  }

  if (!data.is_array()) {
    return mole::fail("Expected an array of node definitions in source.");
  }

  for (const auto& item : data) {
    if (!item.contains("name")) {
      return mole::fail("Node definition is missing 'name' property in source.");
    }

    NodeDefinition node_definition;

    if (item.contains("managers")) {
      if (!item["managers"].is_array()) {
        return mole::fail("Node definition 'managers' property is not an array in source.");
      }

      for (const auto& manager_name : item["managers"]) {
        if (!manager_name.is_string()) {
          return mole::fail("Node definition 'managers' array contains non-string value in source.");
        }

        if (manager_registry.find(manager_name.get<std::string>()) == manager_registry.end()) {
          return mole::fail("Node definition 'managers' array contains unknown manager '" + manager_name.get<std::string>() + "' in source.");
        }
      }

      node_definition.managers = item["managers"].get<std::vector<std::string>>();
    }

    node_definitions.emplace(item["name"].get<std::string>(), std::move(node_definition));
  }

  return mole::ok();
}

Result<void> SceneFactory::save_scene_to(const Scene &scene, std::string_view path) const {
  nlohmann::json data;

  data["scene"] = nlohmann::json::object();
  data["scene"]["type"] = scene.get_scene_type();
  auto scene_ser_result = scene.serialize(data["scene"]);

  if (!scene_ser_result) {
    return scene_ser_result;
  }

  std::string json_string = data.dump(2);
  std::vector<uint8_t> byte_data(json_string.begin(), json_string.end());
  auto r_write_bytes = file_system->write_bytes(path, byte_data);
  if (!r_write_bytes) return r_write_bytes;

  return mole::ok();
}

Result<std::unique_ptr<Scene>> SceneFactory::load_scene_from(const std::string_view scene_source) const {
  nlohmann::json data = nlohmann::json::parse(scene_source, nullptr, false);

  if (data.is_discarded()) {
    return mole::fail("failed to parse JSON from source.");
  }

  if (!data.contains("scene") || !data["scene"].is_object()) {
    return mole::fail("invalid scene JSON: missing 'scene' object in source.");
  }

  const auto& scene_json = data["scene"];

  if (!scene_json.contains("type") || !scene_json["type"].is_string()) {
    return mole::fail("invalid scene JSON: missing 'type' string in source.");
  }

  auto scene_result = create_scene(scene_json["type"].get<std::string>());
  if (!scene_result) {
    return scene_result;
  }

  std::unique_ptr<Scene> scene = std::move(scene_result).value();

  auto deserialization_result = scene->deserialize(scene_json);

  if (!deserialization_result) {
    return mole::fail(deserialization_result.error());
  }

  return std::move(scene);
}

Result<std::unique_ptr<Scene>> SceneFactory::load_scene_from_asset(UUID uuid) const {
   auto r_get_json = asset_manager->acquire<TextAsset>(uuid);
   if (!r_get_json) return mole::fail(r_get_json.error());

   return load_scene_from(r_get_json.value()->as_string_view());
}

} // mole
