#include "resources/material_pool.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "resources/asset_types.hpp"
#include "resources/material.hpp"
#include "resources/material_types.hpp"
#include "resources/resources_context.hpp"

namespace mole {

MaterialPool::MaterialPool(const ResourcesContext& context) : asset_manager(context.asset_manager) {}

bool MaterialPool::is_valid(MaterialID material_id) const noexcept {
  if (material_id == MaterialID::INVALID) {
    return false;
  }
  uint32_t index = material_id.index();
  if (index >= entries.size()) {
    return false;
  }
  const auto& entry = entries[index];
  return entry.alive && entry.generation == material_id.generation();
}

bool MaterialPool::is_valid(UUID uuid) const noexcept {
  if (uuid.is_nil()) {
    return false;
  }
  auto it = uuid_to_id.find(uuid);
  if (it != uuid_to_id.end()) {
    return is_valid(it->second);
  }
  return false;
}

Result<MaterialID> MaterialPool::get_id(UUID uuid) const {
  if (uuid.is_nil()) {
    return mole::fail("UUID is nil");
  }
  auto it = uuid_to_id.find(uuid);
  if (it != uuid_to_id.end() && is_valid(it->second)) {
    return it->second;
  }
  return mole::fail("Material not found for UUID");
}

MaterialID MaterialPool::add(Material material) {
  uint32_t index;
  if (!recycled_indices.empty()) {
    index = recycled_indices.back();
    recycled_indices.pop_back();
  } else {
    index = next_unused_index++;
    entries.emplace_back();
  }

  auto& entry = entries[index];
  entry.alive = true;
  entry.material = std::move(material);
  entry.version++;
  entry.uuid = UUID{};
  entry.source_asset = AssetHandle<MaterialAsset>{};
  entry.synced_asset_version = 0;

  return MaterialID::from(index, entry.generation);
}

Result<MaterialID> MaterialPool::add(const AssetHandle<MaterialAsset>& asset) {
  if (!asset.is_valid()) {
    return mole::fail("Invalid MaterialAsset handle");
  }

  UUID uuid = asset.get_uuid();
  if (!uuid.is_nil()) {
    auto it = uuid_to_id.find(uuid);
    if (it != uuid_to_id.end() && is_valid(it->second)) {
      return it->second;
    }
  }

  Material mat;
  mat.vertex_shader = asset->vertex_shader;
  mat.fragment_shader = asset->fragment_shader;
  mat.properties = asset->properties;

  MaterialID id = add(std::move(mat));
  auto& entry = entries[id.index()];
  entry.uuid = uuid;
  entry.source_asset = asset;
  entry.synced_asset_version = asset.get_version();

  if (!uuid.is_nil()) {
    uuid_to_id[uuid] = id;
  }

  return id;
}

Result<AssetHandle<VertexShaderAsset>> MaterialPool::get_vertex_shader(MaterialID material_id) const {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  return entries[material_id.index()].material.vertex_shader;
}

Result<AssetHandle<VertexShaderAsset>> MaterialPool::get_vertex_shader(UUID uuid) const {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return get_vertex_shader(r_id.value());
}

Result<AssetHandle<FragmentShaderAsset>> MaterialPool::get_fragment_shader(MaterialID material_id) const {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  return entries[material_id.index()].material.fragment_shader;
}

Result<AssetHandle<FragmentShaderAsset>> MaterialPool::get_fragment_shader(UUID uuid) const {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return get_fragment_shader(r_id.value());
}

Result<MaterialProperty> MaterialPool::get_property(MaterialID material_id, std::string_view property) const {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  const auto& props = entries[material_id.index()].material.properties;
  auto it = props.find(property);
  if (it == props.end()) {
    return mole::fail("Property not found");
  }
  return it->second;
}

Result<MaterialProperty> MaterialPool::get_property(UUID uuid, std::string_view property) const {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return get_property(r_id.value(), property);
}

Result<UUID> MaterialPool::get_uuid(MaterialID material_id) const {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  return entries[material_id.index()].uuid;
}

Result<uint32_t> MaterialPool::get_version(MaterialID material_id) const {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  return entries[material_id.index()].version;
}

Result<uint32_t> MaterialPool::get_version(UUID uuid) const {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return get_version(r_id.value());
}

Result<void> MaterialPool::set_property(MaterialID material_id, std::string_view property, MaterialProperty value) {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  auto& entry = entries[material_id.index()];
  auto& props = entry.material.properties;
  if (auto it = props.find(property); it != props.end()) {
    it->second = std::move(value);
  } else {
    props.emplace(std::string(property), std::move(value));
  }
  entry.version++;
  return mole::ok();
}

Result<void> MaterialPool::set_property(UUID uuid, std::string_view property, MaterialProperty value) {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return set_property(r_id.value(), property, std::move(value));
}

Result<void> MaterialPool::set_vertex_shader(MaterialID material_id, AssetHandle<VertexShaderAsset> shader) {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  auto& entry = entries[material_id.index()];
  entry.material.vertex_shader = std::move(shader);
  entry.version++;
  return mole::ok();
}

Result<void> MaterialPool::set_vertex_shader(UUID uuid, AssetHandle<VertexShaderAsset> shader) {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return set_vertex_shader(r_id.value(), std::move(shader));
}

Result<void> MaterialPool::set_fragment_shader(MaterialID material_id, AssetHandle<FragmentShaderAsset> shader) {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  auto& entry = entries[material_id.index()];
  entry.material.fragment_shader = std::move(shader);
  entry.version++;
  return mole::ok();
}

Result<void> MaterialPool::set_fragment_shader(UUID uuid, AssetHandle<FragmentShaderAsset> shader) {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return set_fragment_shader(r_id.value(), std::move(shader));
}

Result<void> MaterialPool::set_source_asset(MaterialID material_id, AssetHandle<MaterialAsset> asset) {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }

  auto& entry = entries[material_id.index()];

  if (!asset.is_valid()) {
    if (!entry.uuid.is_nil()) {
      uuid_to_id.erase(entry.uuid);
      entry.uuid = UUID{};
    }
    entry.source_asset = AssetHandle<MaterialAsset>{};
    entry.synced_asset_version = 0;
    return mole::ok();
  }

  UUID uuid = asset.get_uuid();
  if (uuid.is_nil()) {
    return mole::fail("Source asset has nil UUID");
  }

  auto it = uuid_to_id.find(uuid);
  if (it != uuid_to_id.end() && it->second != material_id && is_valid(it->second)) {
    return mole::fail("UUID already mapped to another valid MaterialID");
  }

  if (!entry.uuid.is_nil() && entry.uuid != uuid) {
    uuid_to_id.erase(entry.uuid);
  }

  uuid_to_id[uuid] = material_id;
  entry.uuid = uuid;
  entry.source_asset = asset;
  entry.synced_asset_version = asset.get_version();

  entry.material.vertex_shader = asset->vertex_shader;
  entry.material.fragment_shader = asset->fragment_shader;
  entry.material.properties = asset->properties;
  entry.version++;

  return mole::ok();
}

Result<AssetHandle<MaterialAsset>> MaterialPool::get_source_asset(MaterialID material_id) const {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }
  return entries[material_id.index()].source_asset;
}

Result<void> MaterialPool::reload_from_source(MaterialID material_id) {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }

  auto& entry = entries[material_id.index()];
  if (!entry.source_asset.is_valid()) {
    return mole::fail("Material does not have a source asset");
  }

  const auto& asset = entry.source_asset;
  entry.material.vertex_shader = asset->vertex_shader;
  entry.material.fragment_shader = asset->fragment_shader;
  entry.material.properties = asset->properties;
  entry.synced_asset_version = asset.get_version();
  entry.version++;

  return mole::ok();
}

Result<void> MaterialPool::reload_from_source(UUID uuid) {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return reload_from_source(r_id.value());
}

void MaterialPool::reload_all_from_source() {
  for (auto& entry : entries) {
    if (entry.alive && entry.source_asset.is_valid()) {
      if (entry.source_asset.get_version() != entry.synced_asset_version) {
        const auto& asset = entry.source_asset;
        entry.material.vertex_shader = asset->vertex_shader;
        entry.material.fragment_shader = asset->fragment_shader;
        entry.material.properties = asset->properties;
        entry.synced_asset_version = asset.get_version();
        entry.version++;
      }
    }
  }
}

Result<void> MaterialPool::destroy(MaterialID material_id) {
  if (!is_valid(material_id)) {
    return mole::fail("MaterialID is invalid");
  }

  uint32_t index = material_id.index();
  auto& entry = entries[index];

  entry.alive = false;
  entry.generation++;

  if (!entry.uuid.is_nil()) {
    uuid_to_id.erase(entry.uuid);
    entry.uuid = UUID{};
  }

  entry.source_asset = AssetHandle<MaterialAsset>{};
  entry.synced_asset_version = 0;
  entry.material = Material{};

  recycled_indices.push_back(index);

  return mole::ok();
}

Result<void> MaterialPool::destroy(UUID uuid) {
  auto r_id = get_id(uuid);
  if (!r_id) return mole::fail(r_id.error());
  return destroy(r_id.value());
}

void MaterialPool::clear() {
  for (auto& entry : entries) {
    entry.alive = false;
    entry.generation++;
    entry.uuid = UUID{};
    entry.source_asset = AssetHandle<MaterialAsset>{};
    entry.material = Material{};
    entry.synced_asset_version = 0;
  }

  uuid_to_id.clear();

  recycled_indices.clear();
  for (uint32_t i = next_unused_index; i > 0; --i) {
    recycled_indices.push_back(i - 1);
  }
}

} // namespace mole
