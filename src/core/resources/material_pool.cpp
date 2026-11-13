#include "resources/material_pool.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "resources/asset_types.hpp"
#include "resources/material.hpp"
#include "resources/material_types.hpp"
#include "resources/resources_context.hpp"

namespace mole {

MaterialPool::MaterialPool(const ResourcesContext& context)
: asset_manager(context.asset_manager) {}

const Material& MaterialPool::get(MaterialID material_id) const {
  assert(is_valid(material_id) && "Attempting to get an invalid Material!"); // TODO return fallback material here
  return materials[material_id.index()];
}

MaterialID MaterialPool::get_id(std::string_view string_material_id) const {
  auto it = string_id_lookup.find(string_material_id);
  if (it != string_id_lookup.end() && is_valid(it->second)) {
    return it->second;
  }
  return MaterialID::INVALID;
}

uint32_t MaterialPool::get_version(MaterialID material_id) const {
  if (is_valid(material_id)) {
    return material_versions[material_id.index()];
  }
  return 0;
}

uint32_t MaterialPool::get_version(std::string_view string_material_id) const {
  MaterialID id = get_id(string_material_id);
  return get_version(id);
}

bool MaterialPool::is_valid(MaterialID material_id) const {
  if (material_id == MaterialID::INVALID) {
    return false;
  }
  uint32_t index = material_id.index();
  return index < generations.size() && generations[index] == material_id.generation();
}

bool MaterialPool::is_valid(std::string_view string_material_id) const {
  auto it = string_id_lookup.find(string_material_id);
  if (it != string_id_lookup.end()) {
    return is_valid(it->second);
  }
  return false;
}

MaterialID MaterialPool::add(std::string_view string_id, Material material) {
  return add(string_id, std::move(material), UUID{});
}

MaterialID MaterialPool::add(std::string_view string_id, Material material, UUID uuid) {
  // If uuid is valid, check if it already exists and is valid
  if (!uuid.is_nil()) {
    auto it = uuid_to_id.find(uuid);
    if (it != uuid_to_id.end()) {
      MaterialID existing_id = it->second;
      if (is_valid(existing_id)) {
        uint32_t index = existing_id.index();
        materials[index] = std::move(material);
        material_versions[index]++;
        if (!string_id.empty()) {
          string_id_lookup[std::string(string_id)] = existing_id;
        }
        return existing_id;
      }
    }
  }

  // If string_id is provided, check if it already exists and is valid
  if (!string_id.empty()) {
    auto it = string_id_lookup.find(string_id);
    if (it != string_id_lookup.end()) {
      MaterialID existing_id = it->second;
      if (is_valid(existing_id)) {
        uint32_t index = existing_id.index();
        materials[index] = std::move(material);
        material_versions[index]++;
        if (!uuid.is_nil()) {
          material_uuids[index] = uuid;
          uuid_to_id[uuid] = existing_id;
        }
        return existing_id;
      }
    }
  }

  // Otherwise, allocate a new slot
  uint32_t index;
  if (!recycled_indices.empty()) {
    index = recycled_indices.back();
    recycled_indices.pop_back();
  } else {
    index = next_unused_index++;
    // Expand the underlying vectors
    materials.emplace_back();
    generations.push_back(0);
    material_versions.push_back(0);
    material_uuids.push_back(UUID{});
  }

  // Move the material into the slot and bump the version
  materials[index] = std::move(material);
  material_versions[index]++;
  material_uuids[index] = uuid;

  // Create the handle and register it in the lookup tables
  MaterialID new_id = MaterialID::from(index, generations[index]);
  if (!string_id.empty()) {
    string_id_lookup[std::string(string_id)] = new_id;
  }
  if (!uuid.is_nil()) {
    uuid_to_id[uuid] = new_id;
  }

  return new_id;
}

MaterialID MaterialPool::acquire_material(const AssetHandle<MaterialAsset>& asset) {
  if (!asset.is_valid()) {
    return MaterialID::INVALID;
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

  return add(uuid.to_string(), std::move(mat), uuid);
}

UUID MaterialPool::get_uuid(MaterialID material_id) const {
  if (!is_valid(material_id)) {
    return UUID{};
  }
  uint32_t index = material_id.index();
  if (index >= material_uuids.size()) {
    return UUID{};
  }
  return material_uuids[index];
}

void MaterialPool::destroy(MaterialID material_id) {
  if (!is_valid(material_id)) {
    return;
  }

  uint32_t index = material_id.index();

  // Invalidate the UUID lookup
  if (index < material_uuids.size() && !material_uuids[index].is_nil()) {
    uuid_to_id.erase(material_uuids[index]);
    material_uuids[index] = UUID{};
  }

  // Invalidate the handle by incrementing the generation
  generations[index]++;
  recycled_indices.push_back(index);

  // Clean up the string lookup map.
  // Since we don't store a reverse lookup (index -> string), we must find it.
  for (auto it = string_id_lookup.begin(); it != string_id_lookup.end(); ++it) {
    if (it->second == material_id) {
      string_id_lookup.erase(it);
      break;
    }
  }
}

void MaterialPool::destroy(std::string_view string_material_id) {
  MaterialID id = get_id(string_material_id);
  if (is_valid(id)) {
    destroy(id);
  }
}

void MaterialPool::clear() {
  for (uint32_t& gen : generations) {
    gen++;
  }

  string_id_lookup.clear();
  uuid_to_id.clear();
  for (UUID& u : material_uuids) {
    u = UUID{};
  }

  recycled_indices.clear();
  for (uint32_t i = next_unused_index; i > 0; --i) {
    recycled_indices.push_back(i - 1);
  }
}

void MaterialPool::set_property(MaterialID material_id, std::string_view property, MaterialProperty value) {
  if (is_valid(material_id)) {
    uint32_t index = material_id.index();

    auto& props = materials[index].properties;
    if (auto it = props.find(property); it != props.end()) {
      it->second = std::move(value);
    } else {
      props.emplace(std::string(property), std::move(value));
    }

    // Bump the version so the renderer knows this material needs a descriptor/uniform update
    material_versions[index]++;
  }
}

void MaterialPool::set_property(std::string_view string_material_id, std::string_view property,
                                MaterialProperty value) {
  MaterialID id = get_id(string_material_id);
  set_property(id, property, std::move(value));
}

} // mole
