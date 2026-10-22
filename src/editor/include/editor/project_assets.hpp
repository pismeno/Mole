#pragma once

#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "editor/editor_context.hpp"
#include "osal/os_context.hpp"
#include "resources/resources_context.hpp"
#include "osal/ifile_system.hpp"
#include "resources/asset_types.hpp"
#include "utils/uuid.hpp"
#include "utils/result.hpp"

namespace mole {

class AssetManager;

class ProjectAssets {
public:
  explicit ProjectAssets(const EditorContext& editor_context)
  : file_system(editor_context.os_context->file_system),
  asset_manager(editor_context.resources_context->asset_manager) {};
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
