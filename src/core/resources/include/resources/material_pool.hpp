#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/material.hpp"
#include "resources/material_types.hpp"
#include "utils/creflector.hpp"
#include "utils/result.hpp"
#include "utils/string_hash.hpp"
#include "utils/uuid.hpp"

namespace mole {

struct ResourcesContext;

class MaterialPool {
public:
  explicit MaterialPool(const ResourcesContext& context);
  ~MaterialPool() = default;

  [[nodiscard]] const Material& get(MaterialID material_id) const;
  [[nodiscard]] MaterialID get_id(std::string_view string_material_id) const;
  [[nodiscard]] uint32_t get_version(MaterialID material_id) const;
  [[nodiscard]] uint32_t get_version(std::string_view string_material_id) const;
  [[nodiscard]] bool is_valid(MaterialID material_id) const;
  [[nodiscard]] bool is_valid(std::string_view string_material_id) const;

  MaterialID add(std::string_view string_id, Material material);
  MaterialID add(std::string_view string_id, Material material, UUID uuid);
  MaterialID acquire_material(const AssetHandle<MaterialAsset>& asset);
  [[nodiscard]] UUID get_uuid(MaterialID material_id) const;

  void destroy(std::string_view string_material_id);
  void destroy(MaterialID material_id);
  void clear();

  void set_property(MaterialID material_id, std::string_view property, MaterialProperty value);
  void set_property(std::string_view string_material_id, std::string_view property, MaterialProperty value);

  template <CReflector TReflector>
  requires(TReflector::is_writing())
  void save_material(TReflector& re, MaterialID material_id) const {
    if (!is_valid(material_id)) return;
    const Material& mat = get(material_id);

    UUID vert_uuid = mat.vertex_shader ? mat.vertex_shader.get_uuid() : UUID{};
    UUID frag_uuid = mat.fragment_shader ? mat.fragment_shader.get_uuid() : UUID{};

    serialize(re, "vertex_shader", vert_uuid);
    serialize(re, "fragment_shader", frag_uuid);

    uint32_t prop_count = static_cast<uint32_t>(mat.properties.size());
    re.begin_array("properties", prop_count);

    for (const auto& [name, prop] : mat.properties) {
      re.begin_object(nullptr);
      std::string name_copy = name;
      serialize(re, "name", name_copy);

      std::visit(
          [&](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, float>) {
              EMaterialPropertyType type = EMaterialPropertyType::Float;
              serialize(re, "type", type);
              float f = val;
              serialize(re, "value", f);
            } else if constexpr (std::is_same_v<T, int>) {
              EMaterialPropertyType type = EMaterialPropertyType::Int;
              serialize(re, "type", type);
              int32_t i = static_cast<int32_t>(val);
              serialize(re, "value", i);
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
              EMaterialPropertyType type = EMaterialPropertyType::Vec2;
              serialize(re, "type", type);
              glm::vec2 v = val;
              serialize(re, "value", v);
            } else if constexpr (std::is_same_v<T, glm::vec3>) {
              EMaterialPropertyType type = EMaterialPropertyType::Vec3;
              serialize(re, "type", type);
              glm::vec3 v = val;
              serialize(re, "value", v);
            } else if constexpr (std::is_same_v<T, glm::vec4>) {
              EMaterialPropertyType type = EMaterialPropertyType::Vec4;
              serialize(re, "type", type);
              glm::vec4 v = val;
              serialize(re, "value", v);
            } else if constexpr (std::is_same_v<T, AssetHandle<ImageAsset>>) {
              EMaterialPropertyType type = EMaterialPropertyType::Texture;
              serialize(re, "type", type);
              UUID tex_uuid = val ? val.get_uuid() : UUID{};
              serialize(re, "value", tex_uuid);
            }
          },
          prop);

      re.end_object();
    }

    re.end_array();
  }

  template <CReflector TReflector>
  requires(!TReflector::is_writing())
  Result<MaterialID> load_material(TReflector& re, UUID material_uuid = {}) {
    UUID vert_uuid{};
    UUID frag_uuid{};
    serialize(re, "vertex_shader", vert_uuid);
    serialize(re, "fragment_shader", frag_uuid);

    Material mat;
    if (!vert_uuid.is_nil()) {
      auto r_vert = asset_manager->acquire<VertexShaderAsset>(vert_uuid);
      if (!r_vert) return mole::fail(r_vert.error());
      mat.vertex_shader = std::move(r_vert).value();
    }

    if (!frag_uuid.is_nil()) {
      auto r_frag = asset_manager->acquire<FragmentShaderAsset>(frag_uuid);
      if (!r_frag) return mole::fail(r_frag.error());
      mat.fragment_shader = std::move(r_frag).value();
    }

    uint32_t prop_count = 0;
    re.begin_array("properties", prop_count);

    for (uint32_t i = 0; i < prop_count; ++i) {
      re.begin_object(nullptr);
      std::string name;
      serialize(re, "name", name);
      EMaterialPropertyType type = EMaterialPropertyType::Float;
      serialize(re, "type", type);

      switch (type) {
        case EMaterialPropertyType::Float: {
          float f = 0.0f;
          serialize(re, "value", f);
          mat.properties.emplace(name, f);
          break;
        }
        case EMaterialPropertyType::Int: {
          int32_t val = 0;
          serialize(re, "value", val);
          mat.properties.emplace(name, static_cast<int>(val));
          break;
        }
        case EMaterialPropertyType::Vec2: {
          glm::vec2 v{0.0f};
          serialize(re, "value", v);
          mat.properties.emplace(name, v);
          break;
        }
        case EMaterialPropertyType::Vec3: {
          glm::vec3 v{0.0f};
          serialize(re, "value", v);
          mat.properties.emplace(name, v);
          break;
        }
        case EMaterialPropertyType::Vec4: {
          glm::vec4 v{0.0f};
          serialize(re, "value", v);
          mat.properties.emplace(name, v);
          break;
        }
        case EMaterialPropertyType::Texture: {
          UUID tex_uuid{};
          serialize(re, "value", tex_uuid);
          AssetHandle<ImageAsset> tex_handle;
          if (!tex_uuid.is_nil()) {
            auto r_tex = asset_manager->acquire<ImageAsset>(tex_uuid);
            if (!r_tex) return mole::fail(r_tex.error());
            tex_handle = std::move(r_tex).value();
          }
          mat.properties.emplace(name, std::move(tex_handle));
          break;
        }
      }

      re.end_object();
    }

    re.end_array();

    std::string id_str = material_uuid.is_nil() ? "" : material_uuid.to_string();
    MaterialID id = add(id_str, std::move(mat), material_uuid);
    return id;
  }

private:
  std::unordered_map<std::string, MaterialID, StringHash, std::equal_to<>> string_id_lookup;
  std::unordered_map<UUID, MaterialID> uuid_to_id;
  std::vector<Material> materials;
  std::vector<uint32_t> generations;
  std::vector<uint32_t> material_versions;
  std::vector<UUID> material_uuids;

  uint32_t next_unused_index{0};
  std::vector<uint32_t> recycled_indices;

  AssetManager* asset_manager;
};

} // mole
