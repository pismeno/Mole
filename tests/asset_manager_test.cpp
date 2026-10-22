#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/resources_context.hpp"
#include "utils/uuid.hpp"

using namespace mole;

TEST_CASE("AssetManager: acquire after unload uses pool without disk dependency") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_asset_mgr_test";
  std::filesystem::create_directories(temp_dir);

  fs->mount("assets", temp_dir);

  OSContext os_ctx{ .file_system = fs.get() };
  ResourcesContext res_ctx{ .os_context = &os_ctx };

  auto r_mgr = AssetManager::create(res_ctx);
  REQUIRE(r_mgr.is_ok());
  auto mgr = std::move(r_mgr).value();

  UUID test_uuid = UUID::generate_v4();
  std::filesystem::path file_path = temp_dir / (test_uuid.to_string() + ".asset");

  std::string payload = "Hello AssetManager";
  AssetHeader header{
      .magic = {'M', 'O', 'L', 'E'},
      .version = ASSET_CURRENT_VERSION,
      .asset_type = AssetType::Text,
      .payload_size = static_cast<uint32_t>(payload.size())
  };

  {
    std::ofstream ofs(file_path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ofs.write(payload.data(), payload.size());
  }

  // 1. Initial acquire loads asset from disk
  auto r_handle = mgr->acquire<TextAsset>(test_uuid);
  REQUIRE(r_handle.is_ok());
  AssetHandle<TextAsset> handle = std::move(r_handle).value();
  CHECK(handle.is_valid());
  CHECK(handle->as_string_view() == "Hello AssetManager");
  CHECK(handle.get_version() == 0);

  // 2. Remove file from disk to simulate cooked asset removal
  std::filesystem::remove(file_path);
  CHECK_FALSE(std::filesystem::exists(file_path));

  // 3. Unload the asset
  mgr->unload(test_uuid);
  CHECK_FALSE(handle.is_valid());
  CHECK(handle->as_string_view() == TextAsset::FALLBACK.as_string_view());
  CHECK(handle.get_version() == 1);

  // 4. Calling acquire after unload should NOT fail with chunk read error;
  // it should return the pool-tracked handle with fallback resolution
  auto r_handle2 = mgr->acquire<TextAsset>(test_uuid);
  REQUIRE(r_handle2.is_ok());
  AssetHandle<TextAsset> handle2 = std::move(r_handle2).value();
  CHECK_FALSE(handle2.is_valid());
  CHECK(handle2->as_string_view() == TextAsset::FALLBACK.as_string_view());
  CHECK(handle2.get_version() == 1);

  // 5. Acquiring with incompatible type fails with pool type mismatch
  auto r_bad_type = mgr->acquire<ImageAsset>(test_uuid);
  CHECK_FALSE(r_bad_type.is_ok());

  // 6. Restoring file and calling reload restores state to Loaded and bumps version
  std::string reloaded_payload = "Reloaded Content";
  header.payload_size = static_cast<uint32_t>(reloaded_payload.size());
  {
    std::ofstream ofs(file_path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ofs.write(reloaded_payload.data(), reloaded_payload.size());
  }

  auto r_reload = mgr->reload(test_uuid);
  REQUIRE(r_reload.is_ok());
  CHECK(handle.is_valid());
  CHECK(handle->as_string_view() == "Reloaded Content");
  CHECK(handle.get_version() == 2);

  // Clean up
  std::filesystem::remove_all(temp_dir);
}

