#include "resources/material_pool.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "resources/material.hpp"
#include "resources/material_types.hpp"

namespace mole {

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
  // If it already exists and is valid, overwrite it
  auto it = string_id_lookup.find(string_id);
  if (it != string_id_lookup.end()) {
    MaterialID existing_id = it->second;
    if (is_valid(existing_id)) {
      uint32_t index = existing_id.index();
      materials[index] = std::move(material);
      material_versions[index]++; // Bump version since data changed
      return existing_id;
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
  }

  // Move the material into the slot and bump the version
  materials[index] = std::move(material);
  material_versions[index]++;

  // Create the handle and register it in the lookup table
  MaterialID new_id = MaterialID::from(index, generations[index]);
  string_id_lookup[std::string(string_id)] = new_id;

  return new_id;
}

void MaterialPool::destroy(MaterialID material_id) {
  if (!is_valid(material_id)) {
    return;
  }

  uint32_t index = material_id.index();

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
  auto it = string_id_lookup.find(string_material_id);
  if (it != string_id_lookup.end()) {
    MaterialID id = it->second;
    string_id_lookup.erase(it); // Erase first to avoid double lookup

    if (is_valid(id)) {
      uint32_t index = id.index();
      generations[index]++;
      recycled_indices.push_back(index);
    }
  }
}

void MaterialPool::clear() {
  for (uint32_t& gen : generations) {
    gen++;
  }

  string_id_lookup.clear();

  recycled_indices.clear();
  for (uint32_t i = next_unused_index; i > 0; --i) {
    recycled_indices.push_back(i - 1);
  }
}

void MaterialPool::set_property(MaterialID material_id, std::string_view property, MaterialProperty value) {
  if (is_valid(material_id)) {
    uint32_t index = material_id.index();

    // Assuming your Material class has a set_property method.
    // Adjust this line based on your actual Material API.
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

void MaterialPool::set_property(std::string_view string_material_id, std::string_view property, MaterialProperty value) {
  MaterialID id = get_id(string_material_id);
  set_property(id, property, std::move(value)); // Fall through to the ID-based function
}

} // mole
