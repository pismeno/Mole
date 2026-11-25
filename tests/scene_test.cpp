#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "node_system/engine_managers.hpp"
#include "node_system/manager_reflector_binder_factory.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/node_types.hpp"
#include "node_system/scene.hpp"
#include "node_system/scene_factory.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_manager.hpp"
#include "resources/material_pool.hpp"
#include "resources/resources_context.hpp"
#include "utils/json_reflector.hpp"
#include "utils/uuid.hpp"

using namespace mole;

TEST_CASE("Scene: destroyed node is invalid and its UUID is released") {
  Scene scene("test_scene");
  UUID uuid = UUID::generate_v4();
  NodeID node = scene.create_node(NodeID::INVALID, "node", uuid);
  REQUIRE(scene.is_node_valid(node));
  CHECK(scene.get_node_by_uuid(uuid) == node);

  scene.destroy_node(node);

  CHECK_FALSE(scene.is_node_valid(node));
  CHECK(scene.get_node_by_uuid(uuid) == NodeID::INVALID);
  CHECK_FALSE(scene.get_uuid(node).is_ok());
}

TEST_CASE("Scene: destroying a parent destroys its children") {
  Scene scene("test_scene");
  NodeID parent = scene.create_node(NodeID::INVALID, "node");
  NodeID child = scene.create_node(parent, "node");
  REQUIRE(scene.get_nodes_parent(child) == parent);
  REQUIRE(scene.get_nodes_children(parent).size() == 1);

  scene.destroy_node(parent);

  CHECK_FALSE(scene.is_node_valid(parent));
  CHECK_FALSE(scene.is_node_valid(child));
}

TEST_CASE("Scene: clear_nodes invalidates old IDs even after slot reuse") {
  Scene scene("test_scene");
  UUID uuid = UUID::generate_v4();
  NodeID old_node = scene.create_node(NodeID::INVALID, "node", uuid);
  REQUIRE(scene.is_node_valid(old_node));

  scene.clear_nodes();
  CHECK_FALSE(scene.is_node_valid(old_node));
  CHECK(scene.get_node_by_uuid(uuid) == NodeID::INVALID);

  NodeID new_node = scene.create_node(NodeID::INVALID, "node", uuid);
  REQUIRE(scene.is_node_valid(new_node));
  CHECK(new_node.index() == old_node.index());
  CHECK_FALSE(scene.is_node_valid(old_node));
}

TEST_CASE("SceneFactory: save_scene skips destroyed nodes") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_scene_save_test";
  std::filesystem::create_directories(temp_dir);
  fs->mount("assets", temp_dir);

  OSContext os_ctx{
    .file_system = fs.get()
  };
  ResourcesContext res_ctx{
    .os_context = &os_ctx
  };

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();
  res_ctx.asset_manager = mgr.get();

  MaterialPool pool(res_ctx);
  res_ctx.material_pool = &pool;

  auto binder_factory = std::make_unique<ManagerReflectorBinderFactory<EngineManagers>>();
  NodeSystemContext node_ctx{
    .resources_context = &res_ctx,
    .manager_reflector_binder_factory = binder_factory.get()
  };

  auto r_sf = SceneFactory::create(node_ctx);
  REQUIRE(r_sf.is_ok());
  auto sf = std::move(r_sf).value();

  REQUIRE(sf->load_node_definitions_from(R"([{ "name": "empty", "managers": [] }])").is_ok());
  REQUIRE(sf->load_scene_definitions_from(R"([{ "name": "empty_scene", "managers": [], "allowed_nodes": ["empty"] }])").is_ok());

  auto r_scene = sf->create_scene("empty_scene");
  REQUIRE(r_scene.is_ok());
  auto scene = std::move(r_scene).value();

  UUID kept_uuid = UUID::generate_v4();
  NodeID dead = scene->create_node(NodeID::INVALID, "empty");
  NodeID kept = scene->create_node(NodeID::INVALID, "empty", kept_uuid);
  REQUIRE(scene->is_node_valid(kept));
  scene->destroy_node(dead);

  nlohmann::json json;
  JsonWriteReflector writer(json);
  sf->save_scene(writer, *scene);

  REQUIRE(json["nodes"].size() == 1);
  CHECK(json["nodes"][0]["id"] == kept_uuid.to_string());

  std::filesystem::remove_all(temp_dir);
}
