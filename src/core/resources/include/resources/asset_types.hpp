#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "resources/asset_handle.hpp"
#include "resources/material.hpp"
#include "utils/string_hash.hpp"

namespace mole {

enum class EAssetType : uint32_t {
  Unkown = 0,
  Text = 1,
  GenericBinary = 2,
  VertexShader = 3,
  FragmentShader = 4,
  ComputeShader = 5,
  GeometryShader = 6,
  TessControlShader = 7,
  TessEvaluationShader = 8,
  Image = 9,
  Scene = 10,
  Material = 11,
};

struct AssetHeader {
  char magic[4]; // "MOLE"
  uint32_t version;
  EAssetType asset_type; // Enum backing type is uint32_t
  uint32_t payload_size; // Size of the actual cooked data
};

constexpr std::string_view ASSET_MAGIC = "MOLE";
constexpr uint32_t ASSET_CURRENT_VERSION = 1;

/**
 * @brief Base class for all assets.
 * All asset structs have to contain ASSET_TYPE
 */
struct Asset {
  virtual ~Asset() = default;
};

struct ImageAsset : Asset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::Image;

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t channels = 0;

  std::vector<uint8_t> pixel_data;

  [[nodiscard]] const inline size_t get_size_bytes() const noexcept { return pixel_data.size(); }

  static const ImageAsset FALLBACK;
};

/**
 * @brief Base class for all binary assets.
 */
struct BinaryAsset : Asset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::GenericBinary;

  std::vector<uint8_t> data;

  [[nodiscard]] const inline size_t get_size_bytes() const noexcept { return data.size(); }

  static const BinaryAsset FALLBACK;
};

/**
 * @brief Base class for all text assets.
 */
struct TextAsset : BinaryAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::Text;

  [[nodiscard]] std::string_view as_string_view() const noexcept;
  [[nodiscard]] std::string as_string() const;

  static const TextAsset FALLBACK;
};

/**
 * @brief Base class for all shader assets. This is not a struct to be used for actual shader assets.
 */
struct ShaderAsset : BinaryAsset {
  [[nodiscard]] const uint32_t* as_32bit_words() const noexcept;
};

struct VertexShaderAsset : ShaderAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::VertexShader;
  static const VertexShaderAsset FALLBACK;
};

struct FragmentShaderAsset : ShaderAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::FragmentShader;
  static const FragmentShaderAsset FALLBACK;
};

struct ComputeShaderAsset : ShaderAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::ComputeShader;
};

struct GeometryShaderAsset : ShaderAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::GeometryShader;
};

struct TessControlShaderAsset : ShaderAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::TessControlShader;
};

struct TessEvaluationShaderAsset : ShaderAsset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::TessEvaluationShader;
};

/**
 * @brief Scene asset containing serialized scene payload.
 */
struct SceneAsset : Asset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::Scene;

  std::vector<uint8_t> payload;

  [[nodiscard]] const inline size_t get_size_bytes() const noexcept { return payload.size(); }
  [[nodiscard]] std::string_view as_string_view() const noexcept {
    if (payload.empty()) return {};
    return std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size());
  }

  static const SceneAsset FALLBACK;
};

/**
 * @brief Material asset containing shader handles and property definitions.
 */
struct MaterialAsset : Asset {
  static constexpr const EAssetType ASSET_TYPE = EAssetType::Material;

  AssetHandle<VertexShaderAsset> vertex_shader;
  AssetHandle<FragmentShaderAsset> fragment_shader;
  std::unordered_map<std::string, MaterialProperty, StringHash, std::equal_to<>> properties;

  static const MaterialAsset FALLBACK;
};

} // mole
