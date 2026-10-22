#pragma once

#include <cstdint>
#include <vector>

#include "rendering/packed_material.hpp"
#include "resources/material_types.hpp"
#include "utils/result.hpp"

namespace mole {

class MaterialPacker;
class MaterialPool;
class MaterialShaderLayoutRegistry;
struct RenderingContext;

class PackedMaterialCache {
 public:
  explicit PackedMaterialCache(RenderingContext& context);
  ~PackedMaterialCache() = default;

  Result<PackedMaterial> get(MaterialID material_id);
 private:
  struct CacheEntry {
    PackedMaterial packed_data;
    uint32_t generation = 0;
    uint32_t version = 0;
    bool is_valid = false;
  };

  std::vector<CacheEntry> cache;

  MaterialShaderLayoutRegistry* material_shader_layout_registry = nullptr;
  MaterialPacker* material_packer = nullptr;
  MaterialPool* material_pool = nullptr;
};

} // mole
