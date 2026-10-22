#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "resources/asset_types.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

struct EditorContext;
class AssetManager;
class IFileSystem;

class ProjectAssets {
 public:
  explicit ProjectAssets(const EditorContext& editor_context);
  ~ProjectAssets() = default;

  template<std::derived_from<Asset> T>
  Result<UUID> resolve(std::string_view path) const { return resolve(path, T::ASSET_TYPE); }
  Result<UUID> resolve(std::string_view path, AssetType requested_type) const;
  Result<std::vector<UUID>> get_cooked_uuids(std::string_view path) const;
  Result<void> remove(std::string_view path) const;
  Result<void> make_file(std::string_view path) const;

 private:
  Result<nlohmann::json> read_meta_json(const std::string& asset_path) const;

  IFileSystem* file_system;
  AssetManager* asset_manager;
};

} // mole
