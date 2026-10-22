#include "rendering/bindless_texture_registry.hpp"

#include <cstdint>
#include <vector>

#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"

namespace mole {

uint32_t BindlessTextureRegistry::get_index(const AssetHandle<ImageAsset>& texture) {
  uint64_t texture_id = texture.get_packed_id();

  auto it = bindless_indices.find(texture_id);
  if (it != bindless_indices.end()) {
    return it->second;
  }

  return allocate_index(texture);
}

Result<uint32_t> BindlessTextureRegistry::get_existing_index(uint64_t texture_id) const {
  auto it = bindless_indices.find(texture_id);
  if (it == bindless_indices.end()) {
    return mole::fail("Texture ID not found in bindless indices");
  }

  return it->second;
}

uint32_t BindlessTextureRegistry::allocate_index(const AssetHandle<ImageAsset>& texture) {
  uint64_t texture_id = texture.get_packed_id();

  auto it = pending_texture_uploads.find(texture_id);
  if (it == pending_texture_uploads.end()) {
    pending_texture_uploads[texture_id] = texture;
  }

  uint32_t bindless_index;
  if (!recycled_indices.empty()) {
    bindless_index = recycled_indices.back();
    recycled_indices.pop_back();
  } else {
    bindless_index = next_unused_index++;
  }
  bindless_indices[texture_id] = bindless_index;
  return bindless_index;
}

void BindlessTextureRegistry::free_index(uint64_t texture_id) {
  auto it = bindless_indices.find(texture_id);
  if (it == bindless_indices.end()) {
    return;
  }

  uint32_t index = it->second;
  bindless_indices.erase(it);
  recycled_indices.push_back(index);
}

std::vector<AssetHandle<ImageAsset>> BindlessTextureRegistry::get_pending_texture_uploads() const {
  std::vector<AssetHandle<ImageAsset>> uploads;
  uploads.reserve(pending_texture_uploads.size());

  for (const auto& [id, handle] : pending_texture_uploads) {
    uploads.push_back(handle);
  }

  return uploads;
}

void BindlessTextureRegistry::clear_pending_texture_uploads() {
  pending_texture_uploads.clear();
}

} // mole
