#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "resources/asset_handle.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "resources/material.hpp"
#include "resources/material_types.hpp"
#include "utils/creflector.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

struct ResourcesContext;

class MaterialPool {
public:
  explicit MaterialPool(const ResourcesContext& context);
  ~MaterialPool() = default;

  template <std::invocable<const Material&> TFunction>
  Result<void> with_material(MaterialID material_id, TFunction&& fn) const {
    if (!is_valid(material_id)) {
      return mole::fail("MaterialID is invalid");
    }
    std::forward<TFunction>(fn)(entries[material_id.index()].material);
    return mole::ok();
  }

  template <std::invocable<const Material&> TFunction>
  Result<void> with_material(UUID uuid, TFunction&& fn) const {
    return with_material(get_id(uuid), fn);
  }

  Result<AssetHandle<VertexShaderAsset>> get_vertex_shader(MaterialID material_id) const;
  Result<AssetHandle<VertexShaderAsset>> get_vertex_shader(UUID uuid) const;
  Result<AssetHandle<FragmentShaderAsset>> get_fragment_shader(MaterialID material_id) const;
  Result<AssetHandle<FragmentShaderAsset>> get_fragment_shader(UUID uuid) const;
  Result<MaterialProperty> get_property(MaterialID material_id, std::string_view property) const;
  Result<MaterialProperty> get_property(UUID uuid, std::string_view property) const;

  Result<UUID> get_uuid(MaterialID material_id) const;

  Result<uint32_t> get_version(MaterialID material_id) const;
  Result<uint32_t> get_version(UUID uuid) const;

  [[nodiscard]] bool is_valid(MaterialID material_id) const noexcept;
  [[nodiscard]] bool is_valid(UUID uuid) const noexcept;

  Result<MaterialID> get_id(UUID uuid) const;

  MaterialID add(Material material);
  Result<MaterialID> add(const AssetHandle<MaterialAsset>& asset);

  Result<void> set_property(MaterialID material_id, std::string_view property, MaterialProperty value);
  Result<void> set_property(UUID uuid, std::string_view property, MaterialProperty value);
  Result<void> set_vertex_shader(MaterialID material_id, AssetHandle<VertexShaderAsset> shader);
  Result<void> set_vertex_shader(UUID uuid, AssetHandle<VertexShaderAsset> shader);
  Result<void> set_fragment_shader(MaterialID material_id, AssetHandle<FragmentShaderAsset> shader);
  Result<void> set_fragment_shader(UUID uuid, AssetHandle<FragmentShaderAsset> shader);

  Result<void> set_source_asset(MaterialID material_id, AssetHandle<MaterialAsset> asset);
  Result<AssetHandle<MaterialAsset>> get_source_asset(MaterialID material_id) const;

  Result<void> reload_from_source(MaterialID material_id);
  Result<void> reload_from_source(UUID uuid);
  void reload_all_from_source();

  Result<void> destroy(MaterialID material_id);
  Result<void> destroy(UUID uuid);
  void clear();

  template <CReflector TReflector>
    requires(TReflector::is_writing())
  Result<void> save_material(TReflector& re, MaterialID material_id) const {
    return with_material(material_id, [&](const Material& mat) {
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
    });
  }

  template <CReflector TReflector>
    requires(!TReflector::is_writing())
  Result<MaterialID> load_material(TReflector& re, UUID material_uuid = {}) {
    if (!asset_manager) {
      return mole::fail("AssetManager is null; cannot load material dependencies");
    }

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

    MaterialID id = add(std::move(mat));
    if (!material_uuid.is_nil()) {
      entries[id.index()].uuid = material_uuid;
      uuid_to_id[material_uuid] = id;
    }
    return id;
  }

private:
  struct MaterialEntry {
    Material material{};
    uint32_t generation{0};
    uint32_t version{0};
    UUID uuid{};
    AssetHandle<MaterialAsset> source_asset{};
    uint32_t synced_asset_version{0};
    bool alive{false};
  };

  std::unordered_map<UUID, MaterialID> uuid_to_id;
  std::vector<MaterialEntry> entries;

  uint32_t next_unused_index{0};
  std::vector<uint32_t> recycled_indices;

  AssetManager* asset_manager;
};

} // mole