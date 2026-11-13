#include "resources/asset_pool.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stb_image.h>
#include <nlohmann/json.hpp>

#include "osal/ifile_system.hpp"
#include "resources/asset_manager.hpp"
#include "resources/asset_types.hpp"
#include "utils/json_reflector.hpp"
#include "utils/payload_utils.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

Result<std::vector<uint8_t>> read_asset_payload(IFileSystem* file_system, UUID uuid) {
  std::string filepath = "assets://" + uuid.to_string() + ".asset";

  auto r_file_size = file_system->get_file_size(filepath);
  if (!r_file_size) {
    return mole::fail(r_file_size.error());
  }

  uint64_t header_size = sizeof(AssetHeader);
  if (r_file_size.value() < header_size) {
    return mole::fail("File smaller than asset header: " + filepath);
  }
  uint64_t payload_size = r_file_size.value() - header_size;

  auto r_read_bytes = file_system->read_chunk(filepath, header_size, payload_size);
  if (!r_read_bytes) {
    return mole::fail(r_read_bytes.error());
  }

  return r_read_bytes;
}

template <>
Result<ImageAsset> AssetPool<ImageAsset>::parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes) {
  int w, h, c;

  stbi_uc* raw_pixels =
      stbi_load_from_memory(raw_bytes.data(), static_cast<int>(raw_bytes.size()), &w, &h, &c, STBI_rgb_alpha);

  if (!raw_pixels) {
    return mole::fail("Failed to decode image data");
  }

  ImageAsset asset;
  asset.width = static_cast<uint32_t>(w);
  asset.height = static_cast<uint32_t>(h);
  asset.channels = 4;

  size_t image_size = w * h * 4;
  asset.pixel_data.assign(raw_pixels, raw_pixels + image_size);

  stbi_image_free(raw_pixels);

  return asset;
}

template <>
Result<SceneAsset> AssetPool<SceneAsset>::parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes) {
  if (!is_json_payload(raw_bytes)) {
    return mole::fail("Binary payload format not yet supported");
  }

  SceneAsset asset;
  asset.payload = std::move(raw_bytes);
  return asset;
}

template <>
Result<MaterialAsset> AssetPool<MaterialAsset>::parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes) {
  if (!is_json_payload(raw_bytes)) {
    return mole::fail("Binary payload format not yet supported");
  }

  std::string_view json_str(reinterpret_cast<const char*>(raw_bytes.data()), raw_bytes.size());
  nlohmann::json json = nlohmann::json::parse(json_str, nullptr, false);
  if (json.is_discarded()) {
    return mole::fail("Failed to parse Material JSON");
  }

  JsonReadReflector reader(json);
  MaterialAsset mat_asset;

  UUID vert_uuid{};
  UUID frag_uuid{};
  serialize(reader, "vertex_shader", vert_uuid);
  serialize(reader, "fragment_shader", frag_uuid);

  if (!vert_uuid.is_nil()) {
    auto r_vert = asset_manager->acquire<VertexShaderAsset>(vert_uuid);
    if (!r_vert) return mole::fail(r_vert.error());
    mat_asset.vertex_shader = std::move(r_vert).value();
  }

  if (!frag_uuid.is_nil()) {
    auto r_frag = asset_manager->acquire<FragmentShaderAsset>(frag_uuid);
    if (!r_frag) return mole::fail(r_frag.error());
    mat_asset.fragment_shader = std::move(r_frag).value();
  }

  uint32_t prop_count = 0;
  reader.begin_array("properties", prop_count);

  for (uint32_t i = 0; i < prop_count; ++i) {
    reader.begin_object(nullptr);
    std::string name;
    serialize(reader, "name", name);
    EMaterialPropertyType type = EMaterialPropertyType::Float;
    serialize(reader, "type", type);

    switch (type) {
      case EMaterialPropertyType::Float: {
        float f = 0.0f;
        serialize(reader, "value", f);
        mat_asset.properties.emplace(name, f);
        break;
      }
      case EMaterialPropertyType::Int: {
        int32_t val = 0;
        serialize(reader, "value", val);
        mat_asset.properties.emplace(name, static_cast<int>(val));
        break;
      }
      case EMaterialPropertyType::Vec2: {
        glm::vec2 v{0.0f};
        serialize(reader, "value", v);
        mat_asset.properties.emplace(name, v);
        break;
      }
      case EMaterialPropertyType::Vec3: {
        glm::vec3 v{0.0f};
        serialize(reader, "value", v);
        mat_asset.properties.emplace(name, v);
        break;
      }
      case EMaterialPropertyType::Vec4: {
        glm::vec4 v{0.0f};
        serialize(reader, "value", v);
        mat_asset.properties.emplace(name, v);
        break;
      }
      case EMaterialPropertyType::Texture: {
        UUID tex_uuid{};
        serialize(reader, "value", tex_uuid);
        AssetHandle<ImageAsset> tex_handle;
        if (!tex_uuid.is_nil()) {
          auto r_tex = asset_manager->acquire<ImageAsset>(tex_uuid);
          if (!r_tex) return mole::fail(r_tex.error());
          tex_handle = std::move(r_tex).value();
        }
        mat_asset.properties.emplace(name, std::move(tex_handle));
        break;
      }
    }

    reader.end_object();
  }

  reader.end_array();

  return mat_asset;
}

} // mole
