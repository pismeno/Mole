#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "resources/asset_handle.hpp"
#include "resources/asset_pool.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

class IFileSystem;
struct ResourcesContext;

/**
 * @brief Central manager for loading, caching, and retrieving assets via type-specific pools.
 */
class AssetManager {

  template <typename> friend class AssetHandle;

 public:
  template <std::derived_from<Asset> T>
  Result<AssetHandle<T>> acquire(UUID uuid) {
    auto* pool = get_pool<T>();

    if (pool->contains(uuid)) {
      return pool->load(uuid);
    }

    for (const auto& [type, other_pool] : asset_pools) {
      if (type != T::ASSET_TYPE && other_pool->contains(uuid)) {
        return mole::fail("Requested asset type does not match the asset type stored in the active pool");
      }
    }

    auto r_asset_header = read_asset_header("assets://" + uuid.to_string() + ".asset");
    if (!r_asset_header) {
      return mole::fail(r_asset_header.error());
    }
    AssetHeader header = std::move(r_asset_header).value();

    if (header.asset_type != T::ASSET_TYPE) {
      return mole::fail("Requested asset type does not match the asset type stored in the physical asset header");
    }

    return pool->load(uuid);
  }

  void unload(UUID uuid);
  void unload(UUID uuid, AssetType type);
  template <std::derived_from<Asset> T>
  void unload(UUID uuid) {
    unload(uuid, T::ASSET_TYPE);
  }

  Result<void> reload(UUID uuid);
  Result<void> reload(UUID uuid, AssetType type);
  template <std::derived_from<Asset> T>
  Result<void> reload(UUID uuid) {
    return reload(uuid, T::ASSET_TYPE);
  }

  uint32_t get_version(UUID uuid);
  uint32_t get_version(UUID uuid, AssetType type);
  template <std::derived_from<Asset> T>
  uint32_t get_version(UUID uuid) {
    return get_version(uuid, T::ASSET_TYPE);
  }

  /**
   * @return Result containing a unique pointer to the created manager, or an error.
   */
  static Result<std::unique_ptr<AssetManager>> create(ResourcesContext& context);

 private:
  AssetManager() = default;
  Result<void> init(ResourcesContext& context);

  /**
   * @return Pointer to the type-specific AssetPool, creating it if it does not yet exist.
   */
  template <typename T>
  AssetPool<T>* get_pool() {
    auto it = asset_pools.find(T::ASSET_TYPE);

    if (it == asset_pools.end()) {
      it = asset_pools.emplace(T::ASSET_TYPE, std::make_unique<AssetPool<T>>(file_system)).first;
    }

    return static_cast<AssetPool<T>*>(it->second.get());
  }

  Result<AssetHeader> read_asset_header(std::string_view virtual_path);

  std::unordered_map<AssetType, std::unique_ptr<IAssetPool>> asset_pools;

  IFileSystem* file_system = nullptr;
};

} // mole
