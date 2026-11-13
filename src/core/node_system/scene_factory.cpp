#include "node_system/scene_factory.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "node_system/managers/drawable_manager.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/scene.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_manager.hpp"
#include "resources/material_pool.hpp"
#include "resources/resources_context.hpp"
#include "utils/json_reflector.hpp"
#include "utils/payload_utils.hpp"
#include "utils/result.hpp"

namespace mole {

SceneFactory::SceneFactory(const NodeSystemContext& context)
    : context(&context),
      asset_manager(context.resources_context->asset_manager),
      material_pool(context.resources_context->material_pool),
      file_system(context.resources_context->os_context->file_system),
      manager_reflector_binder_factory(context.manager_reflector_binder_factory) {}

SceneFactory::~SceneFactory() = default;

void SceneFactory::register_manager_factory(std::string_view name, ManagerFactory factory) {
  std::string s_name(name);
  assert(manager_registry.find(s_name) == manager_registry.end() && "Manager name already registered!");
  manager_registry[s_name] = std::move(factory);
}

Result<std::unique_ptr<SceneFactory>> SceneFactory::create(const NodeSystemContext& context) {
  return std::unique_ptr<SceneFactory>(new SceneFactory(context));
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
    auto mgr = manager_factory();
    scene->add_manager(std::move(mgr));
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

Result<std::unique_ptr<Scene>> SceneFactory::instantiate_scene(AssetHandle<SceneAsset> scene_asset) const {
  if (!scene_asset.is_valid()) {
    return mole::fail("Invalid SceneAsset handle");
  }

  if (!is_json_payload(scene_asset->payload)) {
    return mole::fail("Binary payload format not yet supported");
  }

  std::string_view json_str(reinterpret_cast<const char*>(scene_asset->payload.data()), scene_asset->payload.size());
  nlohmann::json json = nlohmann::json::parse(json_str, nullptr, false);
  if (json.is_discarded()) {
    return mole::fail("Failed to parse scene JSON");
  }

  JsonReadReflector reader(json);
  auto r_scene = load_scene(reader);
  if (!r_scene) {
    return mole::fail(r_scene.error());
  }

  std::unique_ptr<Scene> scene = std::move(r_scene).value();
  scene->set_source_asset(scene_asset);
  return scene;
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
          return mole::fail("Node definition 'managers' array contains unknown manager '" +
                            manager_name.get<std::string>() + "' in source.");
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
          return mole::fail("Node definition 'managers' array contains unknown manager '" +
                            manager_name.get<std::string>() + "' in source.");
        }
      }

      node_definition.managers = item["managers"].get<std::vector<std::string>>();
    }

    node_definitions.emplace(item["name"].get<std::string>(), std::move(node_definition));
  }

  return mole::ok();
}

} // mole
