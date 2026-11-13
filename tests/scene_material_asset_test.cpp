#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "node_system/engine_managers.hpp"
#include "node_system/inode_resolver.hpp"
#include "node_system/manager_reflector_binder_factory.hpp"
#include "node_system/managers/drawable_manager.hpp"
#include "node_system/managers/transform_manager.hpp"
#include "node_system/node_system_context.hpp"
#include "node_system/scene.hpp"
#include "node_system/scene_factory.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/resources_context.hpp"
#include "utils/json_reflector.hpp"
#include "utils/payload_utils.hpp"
#include "utils/uuid.hpp"

using namespace mole;

TEST_CASE("PayloadFormat: Detection of JSON, BOM, whitespace, and binary") {
  SUBCASE("Empty and whitespace payloads") {
    std::vector<uint8_t> empty{};
    CHECK(detect_payload_format(empty) == EPayloadFormat::Binary);
    CHECK_FALSE(is_json_payload(empty));

    std::string spaces = "   \t\r\n  ";
    std::span<const uint8_t> ws_span(reinterpret_cast<const uint8_t*>(spaces.data()), spaces.size());
    CHECK(detect_payload_format(ws_span) == EPayloadFormat::Binary);
    CHECK_FALSE(is_json_payload(ws_span));
  }

  SUBCASE("Standard JSON object and array") {
    std::string json_obj = "{\"key\": \"val\"}";
    std::span<const uint8_t> obj_span(reinterpret_cast<const uint8_t*>(json_obj.data()), json_obj.size());
    CHECK(detect_payload_format(obj_span) == EPayloadFormat::Json);
    CHECK(is_json_payload(obj_span));

    std::string json_arr = "[1, 2, 3]";
    std::span<const uint8_t> arr_span(reinterpret_cast<const uint8_t*>(json_arr.data()), json_arr.size());
    CHECK(detect_payload_format(arr_span) == EPayloadFormat::Json);
    CHECK(is_json_payload(arr_span));
  }

  SUBCASE("JSON with leading whitespace") {
    std::string indented = " \t\r\n { \"test\": 123 }";
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(indented.data()), indented.size());
    CHECK(detect_payload_format(span) == EPayloadFormat::Json);
    CHECK(is_json_payload(span));
  }

  SUBCASE("JSON with UTF-8 BOM") {
    std::vector<uint8_t> bom_json = {0xEF, 0xBB, 0xBF, '{', '"', 'a', '"', ':', '1', '}'};
    CHECK(detect_payload_format(bom_json) == EPayloadFormat::Json);
    CHECK(is_json_payload(bom_json));

    std::vector<uint8_t> bom_ws_json = {0xEF, 0xBB, 0xBF, ' ', '\t', '\n', '[', '1', ']'};
    CHECK(detect_payload_format(bom_ws_json) == EPayloadFormat::Json);
    CHECK(is_json_payload(bom_ws_json));
  }

  SUBCASE("Binary payloads") {
    std::vector<uint8_t> bin_data = {0x00, 0x01, 0x02, 0x03};
    CHECK(detect_payload_format(bin_data) == EPayloadFormat::Binary);
    CHECK_FALSE(is_json_payload(bin_data));

    std::vector<uint8_t> mole_magic = {'M', 'O', 'L', 'E', 0x01, 0x00};
    CHECK(detect_payload_format(mole_magic) == EPayloadFormat::Binary);
    CHECK_FALSE(is_json_payload(mole_magic));
  }
}

TEST_CASE("AssetPool: SceneAsset and MaterialAsset parsing") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_scene_mat_test";
  std::filesystem::create_directories(temp_dir);
  fs->mount("assets", temp_dir);

  OSContext os_ctx{.file_system = fs.get()};
  ResourcesContext res_ctx{.os_context = &os_ctx};

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();

  SUBCASE("SceneAsset loads JSON payload and rejects Binary") {
    // 1. JSON SceneAsset
    UUID json_scene_uuid = UUID::generate_v4();
    std::string scene_json = "{\"scene_type\": \"test\", \"nodes\": []}";
    AssetHeader header{
        .magic = {'M', 'O', 'L', 'E'},
        .version = ASSET_CURRENT_VERSION,
        .asset_type = EAssetType::Scene,
        .payload_size = static_cast<uint32_t>(scene_json.size()),
    };

    {
      std::ofstream ofs(temp_dir / (json_scene_uuid.to_string() + ".asset"), std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
      ofs.write(scene_json.data(), scene_json.size());
    }

    auto r_scene_handle = mgr->acquire<SceneAsset>(json_scene_uuid);
    REQUIRE(r_scene_handle.is_ok());
    auto scene_handle = std::move(r_scene_handle).value();
    CHECK(scene_handle.is_valid());
    CHECK(scene_handle->payload.size() == scene_json.size());

    // 2. Binary SceneAsset (should fail with binary not supported)
    UUID bin_scene_uuid = UUID::generate_v4();
    std::vector<uint8_t> bin_data = {0x00, 0x01, 0x02, 0x03};
    header.payload_size = static_cast<uint32_t>(bin_data.size());
    {
      std::ofstream ofs(temp_dir / (bin_scene_uuid.to_string() + ".asset"), std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
      ofs.write(reinterpret_cast<const char*>(bin_data.data()), bin_data.size());
    }

    auto r_bin_handle = mgr->acquire<SceneAsset>(bin_scene_uuid);
    CHECK_FALSE(r_bin_handle.is_ok());
  }

  SUBCASE("MaterialAsset loads JSON payload and parses properties") {
    UUID vert_uuid = UUID::generate_v4();
    UUID frag_uuid = UUID::generate_v4();

    // Create minimal dummy cooked shaders
    std::string dummy_shader = "// shader";
    AssetHeader shader_hdr{
        .magic = {'M', 'O', 'L', 'E'},
        .version = ASSET_CURRENT_VERSION,
        .asset_type = EAssetType::VertexShader,
        .payload_size = static_cast<uint32_t>(dummy_shader.size()),
    };
    {
      std::ofstream ofs(temp_dir / (vert_uuid.to_string() + ".asset"), std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(&shader_hdr), sizeof(shader_hdr));
      ofs.write(dummy_shader.data(), dummy_shader.size());
    }
    shader_hdr.asset_type = EAssetType::FragmentShader;
    {
      std::ofstream ofs(temp_dir / (frag_uuid.to_string() + ".asset"), std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(&shader_hdr), sizeof(shader_hdr));
      ofs.write(dummy_shader.data(), dummy_shader.size());
    }

    // Create cooked material
    UUID mat_uuid = UUID::generate_v4();
    nlohmann::json mat_json;
    mat_json["vertex_shader"] = vert_uuid.to_string();
    mat_json["fragment_shader"] = frag_uuid.to_string();
    mat_json["properties"] = nlohmann::json::array({
        {{"name", "roughness"}, {"type", 0}, {"value", 0.5f}},
        {{"name", "iterations"}, {"type", 1}, {"value", 4}},
        {{"name", "uv_scale"}, {"type", 2}, {"value", {{"x", 2.0f}, {"y", 2.0f}}}},
        {{"name", "tint"}, {"type", 4}, {"value", {{"x", 1.0f}, {"y", 0.0f}, {"z", 0.0f}, {"w", 1.0f}}}},
    });

    std::string mat_str = mat_json.dump();
    AssetHeader mat_hdr{
        .magic = {'M', 'O', 'L', 'E'},
        .version = ASSET_CURRENT_VERSION,
        .asset_type = EAssetType::Material,
        .payload_size = static_cast<uint32_t>(mat_str.size()),
    };
    {
      std::ofstream ofs(temp_dir / (mat_uuid.to_string() + ".asset"), std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(&mat_hdr), sizeof(mat_hdr));
      ofs.write(mat_str.data(), mat_str.size());
    }

    auto r_mat_handle = mgr->acquire<MaterialAsset>(mat_uuid);
    REQUIRE(r_mat_handle.is_ok());
    auto mat_handle = std::move(r_mat_handle).value();
    CHECK(mat_handle.is_valid());
    CHECK(mat_handle->vertex_shader.is_valid());
    CHECK(mat_handle->fragment_shader.is_valid());
    CHECK(mat_handle->properties.size() == 4);
    CHECK(std::holds_alternative<float>(mat_handle->properties.at("roughness")));
    CHECK(std::get<float>(mat_handle->properties.at("roughness")) == 0.5f);
  }

  std::filesystem::remove_all(temp_dir);
}

TEST_CASE("MaterialPool: add MaterialAsset and UUID lookup") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_matpool_test";
  std::filesystem::create_directories(temp_dir);
  fs->mount("assets", temp_dir);

  OSContext os_ctx{.file_system = fs.get()};
  ResourcesContext res_ctx{.os_context = &os_ctx};

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();
  res_ctx.asset_manager = mgr.get();

  MaterialPool pool(res_ctx);

  SUBCASE("Add invalid handle returns fail") {
    AssetHandle<MaterialAsset> invalid_handle;
    auto r_id = pool.add(invalid_handle);
    CHECK_FALSE(r_id.is_ok());
  }

  SUBCASE("Add valid MaterialAsset caches and returns same MaterialID") {
    UUID mat_uuid = UUID::generate_v4();
    nlohmann::json mat_json;
    mat_json["vertex_shader"] = UUID{}.to_string();
    mat_json["fragment_shader"] = UUID{}.to_string();
    mat_json["properties"] = nlohmann::json::array();
    std::string mat_str = mat_json.dump();

    AssetHeader mat_hdr{
        .magic = {'M', 'O', 'L', 'E'},
        .version = ASSET_CURRENT_VERSION,
        .asset_type = EAssetType::Material,
        .payload_size = static_cast<uint32_t>(mat_str.size()),
    };
    {
      std::ofstream ofs(temp_dir / (mat_uuid.to_string() + ".asset"), std::ios::binary);
      ofs.write(reinterpret_cast<const char*>(&mat_hdr), sizeof(mat_hdr));
      ofs.write(mat_str.data(), mat_str.size());
    }

    auto r_handle = mgr->acquire<MaterialAsset>(mat_uuid);
    REQUIRE(r_handle.is_ok());
    auto handle = std::move(r_handle).value();

    auto r_id1 = pool.add(handle);
    REQUIRE(r_id1.is_ok());
    MaterialID id1 = r_id1.value();
    CHECK(id1 != MaterialID::INVALID);
    CHECK(pool.is_valid(id1));

    // Acquiring again with same asset handle must return exact same MaterialID
    auto r_id2 = pool.add(handle);
    REQUIRE(r_id2.is_ok());
    MaterialID id2 = r_id2.value();
    CHECK(id1 == id2);

    // UUID lookup
    auto r_uuid = pool.get_uuid(id1);
    REQUIRE(r_uuid.is_ok());
    CHECK(r_uuid.value() == mat_uuid);
    auto r_get_id = pool.get_id(mat_uuid);
    REQUIRE(r_get_id.is_ok());
    CHECK(r_get_id.value() == id1);
  }

  std::filesystem::remove_all(temp_dir);
}

TEST_CASE("MaterialPool: save_material and load_material roundtrip") {
  ResourcesContext res_ctx;
  MaterialPool pool_write(res_ctx);
  Material mat;
  mat.properties["float_prop"] = 3.14f;
  mat.properties["int_prop"] = static_cast<int32_t>(42);
  mat.properties["vec2_prop"] = glm::vec2(1.0f, 2.0f);
  mat.properties["vec3_prop"] = glm::vec3(1.0f, 2.0f, 3.0f);
  mat.properties["vec4_prop"] = glm::vec4(1.0f, 2.0f, 3.0f, 4.0f);

  MaterialID id = pool_write.add(std::move(mat));
  REQUIRE(pool_write.is_valid(id));

  // Serialize to JSON
  nlohmann::json json;
  JsonWriteReflector writer(json);
  auto r_save = pool_write.save_material(writer, id);
  REQUIRE(r_save.is_ok());

  CHECK(json.contains("properties"));
  CHECK(json["properties"].is_array());
  CHECK(json["properties"].size() == 5);

  // Deserialize into new MaterialPool
  MaterialPool pool_read(res_ctx);
  JsonReadReflector reader(json);
  UUID mat_uuid = UUID::generate_v4();
  auto r_load = pool_read.load_material(reader, mat_uuid);
  REQUIRE(r_load.is_ok());
  MaterialID loaded_id = r_load.value();
  CHECK(pool_read.is_valid(loaded_id));
  auto r_uuid = pool_read.get_uuid(loaded_id);
  REQUIRE(r_uuid.is_ok());
  CHECK(r_uuid.value() == mat_uuid);
}

TEST_CASE("SceneFactory: instantiate_scene sets source_scene_asset") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_sf_test";
  std::filesystem::create_directories(temp_dir);
  fs->mount("assets", temp_dir);

  OSContext os_ctx{.file_system = fs.get()};
  ResourcesContext res_ctx{.os_context = &os_ctx};

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();
  res_ctx.asset_manager = mgr.get();

  auto pool = std::make_unique<MaterialPool>(res_ctx);
  res_ctx.material_pool = pool.get();

  auto binder_factory = std::make_unique<ManagerReflectorBinderFactory<EngineManagers>>();
  NodeSystemContext node_ctx{
      .resources_context = &res_ctx,
      .manager_reflector_binder_factory = binder_factory.get(),
  };

  auto r_sf = SceneFactory::create(node_ctx);
  REQUIRE(r_sf.is_ok());
  auto sf = std::move(r_sf).value();

  sf->register_manager<TransformManager>();
  sf->register_manager<DrawableManager>();

  // Register node and scene definitions
  std::string node_defs = R"([
    {
      "name": "sprite_2d",
      "managers": ["transform_manager", "drawable_manager"]
    }
  ])";
  REQUIRE(sf->load_node_definitions_from(node_defs).is_ok());

  std::string scene_defs = R"([
    {
      "name": "scene_2d",
      "managers": ["transform_manager", "drawable_manager"],
      "allowed_nodes": ["sprite_2d"]
    }
  ])";
  REQUIRE(sf->load_scene_definitions_from(scene_defs).is_ok());

  // Create cooked SceneAsset
  UUID scene_uuid = UUID::generate_v4();
  UUID node_uuid = UUID::generate_v4();

  nlohmann::json scene_json;
  scene_json["scene_type"] = "scene_2d";
  scene_json["nodes"] = nlohmann::json::array({
      {
          {"id", node_uuid.to_string()},
          {"parent_id", UUID{}.to_string()},
          {"node_type", "sprite_2d"},
          {"managers",
           {
               {"transform_manager",
                {{"position", {{"x", 5.0f}, {"y", 10.0f}}},
                 {"rotation", 0.0f},
                 {"scale", {{"x", 1.0f}, {"y", 1.0f}}},
                 {"shear", {{"x", 0.0f}, {"y", 0.0f}}}}},
               {"drawable_manager",
                {{"visible", true},
                 {"size", {{"x", 2.0f}, {"y", 2.0f}}},
                 {"z_index", 3},
                 {"material_uuid", UUID{}.to_string()}}},
           }},
      },
  });

  std::string scene_str = scene_json.dump();
  AssetHeader scene_hdr{
      .magic = {'M', 'O', 'L', 'E'},
      .version = ASSET_CURRENT_VERSION,
      .asset_type = EAssetType::Scene,
      .payload_size = static_cast<uint32_t>(scene_str.size()),
  };
  {
    std::ofstream ofs(temp_dir / (scene_uuid.to_string() + ".asset"), std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&scene_hdr), sizeof(scene_hdr));
    ofs.write(scene_str.data(), scene_str.size());
  }

  auto r_scene_asset = mgr->acquire<SceneAsset>(scene_uuid);
  REQUIRE(r_scene_asset.is_ok());
  auto scene_asset = std::move(r_scene_asset).value();

  auto r_scene = sf->instantiate_scene(scene_asset);
  REQUIRE(r_scene.is_ok());
  auto scene = std::move(r_scene).value();

  // Verify source asset anchor
  CHECK(scene->get_source_asset() == scene_asset);

  // Verify node was created
  NodeID nid = scene->get_node_by_uuid(node_uuid);
  CHECK(nid != NodeID::INVALID);
  CHECK(scene->is_node_valid(nid));

  std::filesystem::remove_all(temp_dir);
}

TEST_CASE("DrawableManager: reflect writes and reads material_uuid") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_dm_reflect_test";
  std::filesystem::create_directories(temp_dir);
  fs->mount("assets", temp_dir);

  OSContext os_ctx{.file_system = fs.get()};
  ResourcesContext res_ctx{.os_context = &os_ctx};

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();
  res_ctx.asset_manager = mgr.get();

  MaterialPool pool(res_ctx);
  res_ctx.material_pool = &pool;

  NodeSystemContext node_ctx{
      .resources_context = &res_ctx,
  };

  UUID mat_uuid = UUID::generate_v4();
  nlohmann::json mat_json;
  mat_json["vertex_shader"] = UUID{}.to_string();
  mat_json["fragment_shader"] = UUID{}.to_string();
  mat_json["properties"] = nlohmann::json::array();
  std::string mat_str = mat_json.dump();
  AssetHeader mat_hdr{
      .magic = {'M', 'O', 'L', 'E'},
      .version = ASSET_CURRENT_VERSION,
      .asset_type = EAssetType::Material,
      .payload_size = static_cast<uint32_t>(mat_str.size()),
  };
  {
    std::ofstream ofs(temp_dir / (mat_uuid.to_string() + ".asset"), std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&mat_hdr), sizeof(mat_hdr));
    ofs.write(mat_str.data(), mat_str.size());
  }

  auto r_handle = mgr->acquire<MaterialAsset>(mat_uuid);
  REQUIRE(r_handle.is_ok());
  auto r_mat_id = pool.add(r_handle.value());
  REQUIRE(r_mat_id.is_ok());
  MaterialID mat_id = r_mat_id.value();

  DrawableManager dm_write(node_ctx);

  NodeID n0 = NodeID::from(0, 0);
  dm_write.on_node_require_manager(n0);
  REQUIRE(dm_write.set_node_visible(n0, true).is_ok());
  REQUIRE(dm_write.set_node_size(n0, glm::vec2(3.0f, 4.0f)).is_ok());
  REQUIRE(dm_write.set_node_z_index(n0, 7).is_ok());
  REQUIRE(dm_write.set_node_material(n0, mat_id).is_ok());

  // INodeResolver dummy
  class DummyResolver : public INodeResolver {
  public:
    Result<UUID> get_uuid(NodeID) const override { return UUID{}; }
    NodeID get_node_by_uuid(const UUID) const override { return NodeID::INVALID; }
  } dummy_resolver;

  // Serialize to JSON
  nlohmann::json json;
  JsonWriteReflector writer(json);
  dm_write.reflect(n0, writer, dummy_resolver);

  CHECK(json.contains("material_uuid"));
  CHECK(json["material_uuid"] == mat_uuid.to_string());
  CHECK(json["visible"] == true);
  CHECK(json["z_index"] == 7);

  std::filesystem::remove_all(temp_dir);
}

TEST_CASE("MaterialPool: source asset binding, live sync and hot reloading") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_mat_sync_test";
  std::filesystem::create_directories(temp_dir);
  fs->mount("assets", temp_dir);

  OSContext os_ctx{.file_system = fs.get()};
  ResourcesContext res_ctx{.os_context = &os_ctx};

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();
  res_ctx.asset_manager = mgr.get();

  MaterialPool pool(res_ctx);

  auto write_material_file = [&](UUID uuid, float roughness_val) {
    nlohmann::json mat_json;
    mat_json["vertex_shader"] = UUID{}.to_string();
    mat_json["fragment_shader"] = UUID{}.to_string();
    mat_json["properties"] = nlohmann::json::array({
        {{"name", "roughness"}, {"type", 0}, {"value", roughness_val}},
    });
    std::string mat_str = mat_json.dump();
    AssetHeader mat_hdr{
        .magic = {'M', 'O', 'L', 'E'},
        .version = ASSET_CURRENT_VERSION,
        .asset_type = EAssetType::Material,
        .payload_size = static_cast<uint32_t>(mat_str.size()),
    };
    std::ofstream ofs(temp_dir / (uuid.to_string() + ".asset"), std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&mat_hdr), sizeof(mat_hdr));
    ofs.write(mat_str.data(), mat_str.size());
  };

  UUID mat_uuid = UUID::generate_v4();
  write_material_file(mat_uuid, 0.5f);

  auto r_handle = mgr->acquire<MaterialAsset>(mat_uuid);
  REQUIRE(r_handle.is_ok());
  auto handle = std::move(r_handle).value();

  SUBCASE("acquire_material stores and returns valid source asset handle") {
    auto r_id = pool.add(handle);
    REQUIRE(r_id.is_ok());
    MaterialID id = r_id.value();
    CHECK(pool.is_valid(id));
    auto r_src = pool.get_source_asset(id);
    REQUIRE(r_src.is_ok());
    CHECK(r_src.value() == handle);
    CHECK(r_src.value().is_valid());
    auto r_uuid = pool.get_uuid(id);
    REQUIRE(r_uuid.is_ok());
    CHECK(r_uuid.value() == mat_uuid);
  }

  SUBCASE("set_source_asset and get_source_asset binding and unlinking") {
    Material mat;
    MaterialID id = pool.add(std::move(mat));
    CHECK(pool.is_valid(id));
    auto r_src = pool.get_source_asset(id);
    REQUIRE(r_src.is_ok());
    CHECK_FALSE(r_src.value().is_valid());

    // Bind source asset
    auto r_bind = pool.set_source_asset(id, handle);
    REQUIRE(r_bind.is_ok());
    CHECK(pool.get_source_asset(id).value() == handle);
    CHECK(pool.get_uuid(id).value() == mat_uuid);
    CHECK(pool.get_id(mat_uuid).value() == id);
    auto r_prop = pool.get_property(id, "roughness");
    REQUIRE(r_prop.is_ok());
    CHECK(std::get<float>(r_prop.value()) == 0.5f);

    // Conflict check: another material cannot bind to the same UUID
    MaterialID id2 = pool.add(Material{});
    auto r_conflict = pool.set_source_asset(id2, handle);
    CHECK_FALSE(r_conflict.is_ok());

    // Unlink source asset
    auto r_unlink = pool.set_source_asset(id, AssetHandle<MaterialAsset>{});
    REQUIRE(r_unlink.is_ok());
    CHECK_FALSE(pool.get_source_asset(id).value().is_valid());
    CHECK(pool.get_uuid(id).value().is_nil());

    // Now id2 can bind to handle
    auto r_bind2 = pool.set_source_asset(id2, handle);
    CHECK(r_bind2.is_ok());
    CHECK(pool.get_source_asset(id2).value() == handle);
  }

  SUBCASE("Invalid material ID for set_source_asset and reload_from_source") {
    CHECK_FALSE(pool.set_source_asset(MaterialID::INVALID, handle).is_ok());
    CHECK_FALSE(pool.reload_from_source(MaterialID::INVALID).is_ok());
    CHECK_FALSE(pool.get_source_asset(MaterialID::INVALID).is_ok());

    MaterialID unlinked_id = pool.add(Material{});
    CHECK_FALSE(pool.reload_from_source(unlinked_id).is_ok());
  }

  SUBCASE("Live sync on AssetManager::reload") {
    auto r_id = pool.add(handle);
    REQUIRE(r_id.is_ok());
    MaterialID id = r_id.value();
    CHECK(pool.is_valid(id));
    auto r_prop1 = pool.get_property(id, "roughness");
    REQUIRE(r_prop1.is_ok());
    CHECK(std::get<float>(r_prop1.value()) == 0.5f);
    uint32_t v1 = pool.get_version(id).value();

    // Overwrite file on disk with new roughness
    write_material_file(mat_uuid, 0.85f);

    // Reload asset through AssetManager
    REQUIRE(mgr->reload<MaterialAsset>(mat_uuid).is_ok());

    // reload_all_from_source triggers sync and increments material version
    pool.reload_all_from_source();
    uint32_t v2 = pool.get_version(id).value();
    CHECK(v2 > v1);

    // get_property yields newly reloaded property
    auto r_prop2 = pool.get_property(id, "roughness");
    REQUIRE(r_prop2.is_ok());
    CHECK(std::get<float>(r_prop2.value()) == 0.85f);
  }

  SUBCASE("reload_from_source manual trigger") {
    auto r_id = pool.add(handle);
    REQUIRE(r_id.is_ok());
    MaterialID id = r_id.value();
    CHECK(pool.is_valid(id));
    auto r_prop1 = pool.get_property(id, "roughness");
    REQUIRE(r_prop1.is_ok());
    CHECK(std::get<float>(r_prop1.value()) == 0.5f);

    // Modify runtime property
    REQUIRE(pool.set_property(id, "roughness", 0.123f).is_ok());
    auto r_prop2 = pool.get_property(id, "roughness");
    REQUIRE(r_prop2.is_ok());
    CHECK(std::get<float>(r_prop2.value()) == 0.123f);
    uint32_t v_mod = pool.get_version(id).value();

    // Trigger reload_from_source
    REQUIRE(pool.reload_from_source(id).is_ok());
    uint32_t v_reloaded = pool.get_version(id).value();
    CHECK(v_reloaded > v_mod);
    auto r_prop3 = pool.get_property(id, "roughness");
    REQUIRE(r_prop3.is_ok());
    CHECK(std::get<float>(r_prop3.value()) == 0.5f);
  }

  std::filesystem::remove_all(temp_dir);
}
