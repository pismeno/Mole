#pragma once

#include <resources/asset_handle.hpp>
#include <vector>
#include <string>
#include <unordered_map>

#include <resources/asset_types.hpp>
#include <osal/ifile_system.hpp>
#include <osal/os_context.hpp>
#include <utils/result.hpp>
#include <utils/string_hash.hpp>
#include "utils/uuid.hpp"

namespace mole {

class IAssetPool { public: virtual ~IAssetPool() = default; };

template<typename T>
class AssetPool : public IAssetPool {
 public:
  AssetPool(IFileSystem* file_system) : file_system(file_system) {};

  Result<AssetHandle<T>> load(UUID uuid) {
    // Check if the asset is already loaded
    auto it = cache.find(uuid);
    if (it != cache.end()) {
      uint32_t index = it->second;
      ref_counts[index]++;
      return AssetHandle<T>(this, index, generations[index]);
    }

    std::string filepath = "assets://" + uuid.to_string() + ".asset";

    auto r_file_size = file_system->get_file_size(filepath);
    if (!r_file_size) {
      return mole::fail(r_file_size.error());
    }

    uint64_t header_size = sizeof(AssetHeader);
    uint64_t payload_size = r_file_size.value() - header_size;

    // Perform I/O before touching any internal index state
    auto r_read_bytes = file_system->read_chunk(filepath, header_size, payload_size);
    if (!r_read_bytes) {
      return mole::fail(r_read_bytes.error());
    }

    auto r_asset = parse_asset(filepath, std::move(r_read_bytes).value());
    if (!r_asset) {
      return mole::fail(r_asset.error());
    }

    // I/O succeeded: now acquire/allocate an index safely
    uint32_t index;
    if (!free_indices.empty()) {
      index = free_indices.back();
      free_indices.pop_back();
    } else {
      index = static_cast<uint32_t>(data.size());
      data.emplace_back();
      generations.push_back(1);
      ref_counts.push_back(0);
      filepaths.emplace_back();
    }

    data[index] = std::move(r_asset).value();
    filepaths[index] = uuid;
    cache.try_emplace(filepaths[index], index);
    ref_counts[index] = 1;

    return AssetHandle<T>(this, index, generations[index]);
  }

  T *resolve(uint32_t index, uint32_t generation) {
    if (index >= data.size()) return nullptr;
    if (generations[index] != generation) return nullptr;
    return &data[index];
  }

  void add_ref(uint32_t index, uint32_t generation) {
    if (index < data.size() && generations[index] == generation) {
      ref_counts[index]++;
    }
  }

  void release(uint32_t index, uint32_t generation) {
    if (index >= data.size() || generations[index] != generation) return;

    ref_counts[index]--;

    if (ref_counts[index] == 0) {
      cache.erase(filepaths[index]);
      data[index] = T{};
      generations[index]++;
      free_indices.push_back(index);
    }
  }

 protected:
  // 1. Declare the template method inside the class without the inline definition
  Result<T> parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes);

 private:
  std::vector<T> data;
  std::vector<uint32_t> generations;
  std::vector<uint32_t> ref_counts;
  std::vector<UUID> filepaths;
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