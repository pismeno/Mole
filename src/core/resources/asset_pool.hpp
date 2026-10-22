#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

class IFileSystem;

Result<std::vector<uint8_t>> read_asset_payload(IFileSystem* file_system, UUID uuid);

class IAssetPool {
 public:
  virtual ~IAssetPool() = default;

  virtual void mark_missing(UUID uuid) = 0;
  virtual Result<void> reload(UUID uuid) = 0;
  virtual bool contains(UUID uuid) const = 0;
  virtual bool is_loaded(UUID uuid) const = 0;
  virtual uint32_t get_version(UUID uuid) const = 0;
};

template<typename T>
class AssetPool : public IAssetPool {
 public:
  AssetPool(IFileSystem* file_system) : file_system(file_system) {}

  Result<AssetHandle<T>> load(UUID uuid) {
    // Check if the asset is already loaded
    auto it = cache.find(uuid);
    if (it != cache.end()) {
      uint32_t index = it->second;
      assets[index].ref_count++;
      return AssetHandle<T>(this, index, assets[index].generation);
    }

    auto r_asset = load_asset_from_file(uuid);
    if (!r_asset) {
      return mole::fail(r_asset.error());
    }

    // I/O succeeded: now acquire/allocate an index safely
    uint32_t index;
    if (!free_indices.empty()) {
      index = free_indices.back();
      free_indices.pop_back();
    } else {
      index = static_cast<uint32_t>(assets.size());
      assets.emplace_back();
      uuids.emplace_back();
    }

    assets[index].asset = std::move(r_asset).value();
    uuids[index] = uuid;
    cache.try_emplace(uuids[index], index);
    assets[index].ref_count = 1;

    return AssetHandle<T>(this, index, assets[index].generation);
  }

  Result<void> reload(UUID uuid) override {
    auto it = cache.find(uuid);
    if (it == cache.end()) {
      return mole::fail("Asset not found");
    }

    uint32_t index = it->second;

    auto r_asset = load_asset_from_file(uuid);
    if (!r_asset) {
      assets[index].state = AssetState::MissingOnDisk;
      return mole::fail(r_asset.error());
    }
    assets[index].asset = std::move(r_asset).value();
    assets[index].version++;
    return mole::ok();
  }

  Result<T> load_asset_from_file(UUID uuid) {
    auto r_read_bytes = read_asset_payload(file_system, uuid);
    if (!r_read_bytes) {
      if (contains(uuid)) {
        mark_missing(uuid);
      }

      return mole::fail(r_read_bytes.error());
    }

    std::string filepath = "assets://" + uuid.to_string() + ".asset";
    return parse_asset(filepath, std::move(r_read_bytes).value());
  }

  void mark_missing(UUID uuid) override {
    auto it = cache.find(uuid);
    if (it != cache.end()) {
      uint32_t index = it->second;
      assets[index].state = AssetState::MissingOnDisk;
      assets[index].asset = T{};
      assets[index].version++;
    }
  }

  const T& resolve(uint32_t index, uint32_t generation) {
    if (index >= assets.size()) return fallback_asset;
    if (assets[index].generation != generation) return fallback_asset;
    if (assets[index].state == AssetState::MissingOnDisk) return fallback_asset;
    return assets[index].asset;
  }

  void add_ref(uint32_t index, uint32_t generation) {
    if (index < assets.size() && assets[index].generation == generation) {
      assets[index].ref_count++;
    }
  }

  void release(uint32_t index, uint32_t generation) {
    if (index >= assets.size() || assets[index].generation != generation) return;

    assets[index].ref_count--;

    if (assets[index].ref_count == 0) {
      cache.erase(uuids[index]);
      assets[index].asset = T{};
      assets[index].version = 0;
      assets[index].generation++;
      assets[index].state = AssetState::Unloaded;
      free_indices.push_back(index);
    }
  }

  bool contains(UUID uuid) const override {
    return cache.contains(uuid);
  }

  bool is_loaded(UUID uuid) const override {
    auto it = cache.find(uuid);
    if (it == cache.end()) return false;

    uint32_t index = it->second;
    return assets[index].state == AssetState::Loaded;
  }

  bool is_loaded(uint32_t index, uint32_t generation) const {
    if (index >= assets.size()) return false;
    if (assets[index].generation != generation) return false;
    return assets[index].state == AssetState::Loaded;
  }

  uint32_t get_version(UUID uuid) const override {
    auto it = cache.find(uuid);
    if (it == cache.end()) return 0;
    return assets[it->second].version;
  }

  UUID get_uuid(uint32_t index) const {
    return uuids[index];
  }

  uint32_t get_version(uint32_t index) const {
    return assets[index].version;
  }

 protected:
  Result<T> parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes);

 private:
  enum class AssetState {
    Loaded,
    MissingOnDisk,
    Unloaded,
  };

  struct AssetSlot {
    T asset;
    uint32_t generation = 1;
    uint32_t ref_count = 0;
    uint32_t version = 0;
    AssetState state = AssetState::Loaded;
  };

  std::vector<AssetSlot> assets;
  T fallback_asset = T::FALLBACK;

  std::vector<UUID> uuids;
  std::vector<uint32_t> free_indices;
  std::unordered_map<UUID, uint32_t> cache;

  IFileSystem* file_system;
};

template<typename T>
inline Result<T> AssetPool<T>::parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes) {
  T asset;
  asset.data = std::move(raw_bytes);
  return asset;
}

template <>
Result<ImageAsset> AssetPool<ImageAsset>::parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes);

} // mole
