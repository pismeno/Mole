#include "rendering/material_packer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <variant>
#include <vector>

#include "rendering/bindless_texture_registry.hpp"
#include "rendering/material_shader_layout.hpp"
#include "rendering/packed_material.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "resources/material.hpp"

namespace mole {

MaterialPacker::MaterialPacker(RenderingContext& rendering_context)
    : bindless_texture_registry(rendering_context.bindless_texture_registry) {}

PackedMaterial MaterialPacker::pack(const Material& material, const MaterialShaderLayout& layout) const {
  std::vector<std::byte> buffer(layout.total_size, std::byte{0});
  std::vector<AssetHandle<ImageAsset>> referenced_textures;

  for (const auto& [prop_name, prop_value] : material.properties) {
    auto it = layout.properties.find(prop_name); // Look up the property in the layout map

    if (it == layout.properties.end()) { // If the shader compiler optimized this property out, safely skip it.
      continue;
    }

    const ShaderPropertyLayout& prop_layout = it->second;

    std::byte* dst_ptr = buffer.data() + prop_layout.offset; // Pointer to the exact byte offset inside our buffer

    // Unpack the variant and copy the bytes
    std::visit([&](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, AssetHandle<ImageAsset>>) {
        // Use the internal registry pointer to map the texture to an integer
        uint32_t bindless_index = bindless_texture_registry->get_index(arg);
        std::memcpy(dst_ptr, &bindless_index, sizeof(uint32_t));
      }
      else {
        // Copy float, int, glm::vec2, glm::vec3, or glm::vec4
        std::memcpy(dst_ptr, &arg, sizeof(T));
      }
    }, prop_value);
  }

  return { buffer };
}

} // mole
