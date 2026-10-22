#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

class BindlessTextureRegistry {
 public:
  explicit BindlessTextureRegistry() = default;
  ~BindlessTextureRegistry() = default;

  /**
   * @brief Get the bindless index for the given texture. If the texture doesn't have an index it allocates a new one for it,
   * and creates a pending texture upload for it.
   */
  uint32_t get_index(const AssetHandle<ImageAsset>& texture);

  Result<uint32_t> get_existing_index(uint64_t texture_id) const;
  inline Result<uint32_t> get_existing_index(const AssetHandle<ImageAsset>& texture) const { return get_existing_index(texture.get_packed_id()); }

  /**
   * @brief Free the bindless index for the given texture.
   */
  void free_index(uint64_t texture_id);

  /**
   * @brief Get all pending texture uploads.
   */
  [[nodiscard]] std::vector<AssetHandle<ImageAsset>> get_pending_texture_uploads() const;

  /**
   * @brief Clear all pending texture uploads. Should be done after resolving all pending texture uploads.
   */
  void clear_pending_texture_uploads();
 private:
  uint32_t allocate_index(const AssetHandle<ImageAsset>& texture);

  std::unordered_map<uint64_t, uint32_t> bindless_indices;
  std::vector<uint32_t> recycled_indices;
  uint32_t next_unused_index {0};

  std::unordered_map<uint64_t, AssetHandle<ImageAsset>> pending_texture_uploads;
};

} // mole