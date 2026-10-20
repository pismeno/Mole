#pragma once

#include <string_view>

#include "editor/editor_context.hpp"
#include "osal/os_context.hpp"
#include "osal/ifile_system.hpp"
#include "resources/asset_types.hpp"
#include "utils/uuid.hpp"
#include "utils/result.hpp"

namespace mole {

class ProjectAssets {
public:
  explicit ProjectAssets(const EditorContext& editor_context)
    : file_system(editor_context.os_context->file_system) {}
  ~ProjectAssets() = default;

  template<std::derived_from<Asset> T>
  Result<UUID> resolve(std::string_view path) const { return resolve(path, T::ASSET_TYPE); }
  Result<UUID> resolve(std::string_view path, AssetType requested_type) const;
  //Result<void> remove(std::string_view path) const;
  //Result<void> make_file(std::string_view path) const;
private:
  IFileSystem* file_system;
};

} // mole