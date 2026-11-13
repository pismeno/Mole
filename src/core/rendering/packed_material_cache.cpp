#include "rendering/packed_material_cache.hpp"

#include <cstdint>
#include <utility>
#include <vector>

#include "rendering/material_packer.hpp"
#include "rendering/material_shader_layout.hpp"
#include "rendering/material_shader_layout_registry.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/material_types.hpp"
#include "resources/resources_context.hpp"
#include "utils/result.hpp"

namespace mole {

PackedMaterialCache::PackedMaterialCache(RenderingContext& context)
    : material_shader_layout_registry(context.material_shader_layout_registry),
      material_packer(context.material_packer),
      material_pool(context.resources_context->material_pool) {}

Result<PackedMaterial> PackedMaterialCache::get(MaterialID material_id) {
  if (!material_pool->is_valid(material_id)) {
    return mole::fail("MaterialID is invalid.");
  }

  uint32_t index = material_id.index();
  uint32_t pool_generation = material_id.generation();
  auto r_pool_version = material_pool->get_version(material_id);
  if (!r_pool_version) return mole::fail(r_pool_version.error());
  uint32_t pool_version = r_pool_version.value();

  auto r_vert = material_pool->get_vertex_shader(material_id);
  if (!r_vert) return mole::fail(r_vert.error());
  auto r_frag = material_pool->get_fragment_shader(material_id);
  if (!r_frag) return mole::fail(r_frag.error());

  uint32_t vert_version = r_vert.value().get_version();
  uint32_t frag_version = r_frag.value().get_version();

  if (index >= cache.size()) {
    cache.resize(index + 1);
  }

  CacheEntry& entry = cache[index];

  if (!entry.is_valid || entry.generation != pool_generation || entry.version < pool_version ||
      entry.vert_shader_version < vert_version || entry.frag_shader_version < frag_version) {
    auto r_get_layout = material_shader_layout_registry->get(r_frag.value());
    if (!r_get_layout) return mole::fail(r_get_layout.error());
    MaterialShaderLayout layout = std::move(r_get_layout).value();

    auto r = material_pool->with_material(
        material_id, [&](const Material& material) { entry.packed_data = material_packer->pack(material, layout); });
    if (!r) return mole::fail(r.error());

    entry.generation = pool_generation;
    entry.version = pool_version;
    entry.vert_shader_version = vert_version;
    entry.frag_shader_version = frag_version;
    entry.is_valid = true;
  }

  return entry.packed_data;
}

} // namespace mole
