#include "editor/project_assets.hpp"
#include <filesystem>
#include "resources/asset_types.hpp"
#include "resources/asset_manager.hpp"

namespace mole {

Result<nlohmann::json> ProjectAssets::read_meta_json(const std::string& asset_path) const {
  auto r_read_bytes = file_system->read_bytes(asset_path + ".meta");
  if (!r_read_bytes) return mole::fail("Failed to read asset's .meta file: " + r_read_bytes.error());

  std::vector<uint8_t> bytes = std::move(r_read_bytes).value();
  nlohmann::json json = nlohmann::json::parse(bytes, nullptr, false);

  if (json.is_discarded()) {
    return mole::fail("Failed to parse JSON from asset's .meta file");
  }

  return json;
}

Result<UUID> ProjectAssets::resolve(std::string_view path, AssetType requested_type) const {
  std::string asset_path = std::string("project://assets/").append(path);
  auto r_is_file = file_system->is_file(asset_path);

  if (!r_is_file || !r_is_file.value() || asset_path.ends_with(".meta")) {
    return mole::fail("Asset path does not represent an asset");
  }

  auto r_json = read_meta_json(asset_path);
  if (!r_json) return mole::fail(r_json.error());

  const nlohmann::json& json = r_json.value();
  auto cooked_assets_it = json.find("cooked_assets");

  if (cooked_assets_it == json.end() || !cooked_assets_it->is_array()) {
    return mole::fail("Asset's .meta file does not contain an array of cooked assets");
  }

  for (const auto& cooked_asset_recipe : *cooked_assets_it) {
    if (!cooked_asset_recipe["uuid"].is_string() || !cooked_asset_recipe["type"].is_number()) {
      continue;
    }

    if (cooked_asset_recipe["type"].get<int>() == static_cast<int>(requested_type)) {
      return UUID::from_string(cooked_asset_recipe["uuid"].get<std::string>());
    }
  }

  return mole::fail("Asset does not contain a cooked asset of the requested type");
}

Result<std::vector<UUID>> ProjectAssets::get_cooked_uuids(std::string_view path) const {
  std::string asset_path = std::string("project://assets/").append(path);
  auto r_json = read_meta_json(asset_path);
  if (!r_json) return mole::fail(r_json.error());

  const nlohmann::json& json = r_json.value();
  auto cooked_assets_it = json.find("cooked_assets");
  if (cooked_assets_it == json.end() || !cooked_assets_it->is_array()) {
    return mole::fail("Asset's .meta file does not contain an array of cooked assets");
  }

  std::vector<UUID> uuids;
  for (const auto& cooked_asset_recipe : *cooked_assets_it) {
    if (!cooked_asset_recipe["uuid"].is_string()) continue;
    uuids.push_back(UUID::from_string(cooked_asset_recipe["uuid"].get<std::string>()));
  }

  return uuids;
}

Result<void> ProjectAssets::remove(std::string_view path) const {
  std::string asset_path = std::string("project://assets/").append(path);
  auto r_is_file = file_system->is_file(asset_path);

  if (!r_is_file || !r_is_file.value() || asset_path.ends_with(".meta")) {
    return mole::fail("Asset path does not represent an asset");
  }

  // Attempt to clean up meta file and cooked assets if meta exists
  if (auto r_json = read_meta_json(asset_path)) {
    const nlohmann::json& json = r_json.value();
    auto cooked_assets_it = json.find("cooked_assets");

    if (cooked_assets_it != json.end() && cooked_assets_it->is_array()) {
      for (const auto& cooked_asset_recipe : *cooked_assets_it) {
        if (!cooked_asset_recipe["uuid"].is_string()) continue;

        UUID uuid = UUID::from_string(cooked_asset_recipe["uuid"].get<std::string>());

        if (asset_manager) {
          asset_manager->unload(uuid);
        }

        auto r_remove_cooked = file_system->remove("assets://" + uuid.to_string() + ".asset");
        if (!r_remove_cooked) return mole::fail(r_remove_cooked.error());
      }
    }

    auto r_remove_meta = file_system->remove(asset_path + ".meta");
    if (!r_remove_meta) return mole::fail(r_remove_meta.error());
  }

  // Finally, remove the asset itself
  auto r_remove_asset = file_system->remove(asset_path);
  if (!r_remove_asset) return mole::fail(r_remove_asset.error());

  return mole::ok();
}

Result<void> ProjectAssets::make_file(std::string_view path) const {
  std::string asset_path = std::string("project://assets/").append(path);
  auto r_make = file_system->make_file(asset_path);
  if (!r_make) return mole::fail(r_make.error());
  return mole::ok();
}

} // mole
