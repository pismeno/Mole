#include "editor/project_assets.hpp"

#include <filesystem>

#include <nlohmann/json.hpp>

#include "resources/asset_types.hpp"

namespace mole {

Result<UUID> ProjectAssets::resolve(std::string_view path, AssetType requested_type) const {
  auto r_is_file = file_system->is_file(path);
  if (!r_is_file || !r_is_file.value() || path.ends_with(".meta")) {
    return mole::fail("Asset path does not represent an asset");
  }

  auto r_read_bytes = file_system->read_bytes(std::string(path) + ".meta");
  if (!r_read_bytes) return mole::fail("Failed to read asset's .meta file: " + r_read_bytes.error());
  std::vector<uint8_t> bytes = std::move(r_read_bytes).value();
  nlohmann::json json = nlohmann::json::parse(bytes, nullptr, false);

  if (json.is_discarded()) {
    return mole::fail("Failed to parse JSON from asset's .meta file");
  }

  if (!json["cooked_assets"].is_array()) {
    return mole::fail("Asset's .meta file does not contain an array of cooked assets");
  }

  for (auto& cooked_asset_recipe : json["cooked_assets"]) {
    if (!cooked_asset_recipe["uuid"].is_string()) {
      continue;
    }
    int type_val = -1;
    if (cooked_asset_recipe["type"].is_number()) {
      type_val = cooked_asset_recipe["type"].get<int>();
    } else if (cooked_asset_recipe["type"].is_string()) {
      try {
        type_val = std::stoi(cooked_asset_recipe["type"].get<std::string>());
      } catch (...) {
        continue;
      }
    } else {
      continue;
    }
    if (type_val == static_cast<int>(requested_type)) {
      return UUID::from_string(cooked_asset_recipe["uuid"].get<std::string>());
    }
  }

  return mole::fail("Asset does not contain a cooked asset of the requested type");
}

} // mole
