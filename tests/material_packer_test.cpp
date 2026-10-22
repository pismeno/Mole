#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <doctest/doctest.h>
#include <glm/vec3.hpp>

#include "rendering/bindless_texture_registry.hpp"
#include "rendering/material_packer.hpp"
#include "rendering/material_shader_layout.hpp"
#include "rendering/packed_material.hpp"
#include "rendering/rendering_context.hpp"
#include "resources/asset_handle.hpp"
#include "resources/asset_types.hpp"
#include "resources/material.hpp"

namespace mole {

TEST_CASE("MaterialPacker serializes properties to correct byte offsets") {
  // 1. Setup dependencies
  BindlessTextureRegistry registry;
  RenderingContext context;

  // Assuming bindless_texture_registry is a pointer in RenderingContext based on your previous code
  context.bindless_texture_registry = &registry;

  MaterialPacker baker(context);

  // 2. Define a strict layout mapping
  MaterialShaderLayout layout;
  layout.properties["albedo"] = {0, 12};        // vec3 (12 bytes)
  layout.properties["roughness"] = {12, 4};     // float (4 bytes)
  layout.properties["albedo_map"] = {16, 4};    // bindless index (uint32_t, 4 bytes)
  layout.total_size = 20;

  // 3. Setup the material with expected data
  Material material;
  material.properties["albedo"] = glm::vec3(1.0f, 0.5f, 0.25f);
  material.properties["roughness"] = 0.8f;

  AssetHandle<ImageAsset> dummy_texture;
  material.properties["albedo_map"] = dummy_texture;

  uint32_t expected_texture_index = registry.get_index(dummy_texture);

  // 4. Perform the bake
  PackedMaterial packed_material = baker.pack(material, layout);
  std::vector<std::byte> baked_bytes = packed_material.data;

  // 5. Assertions
  REQUIRE(baked_bytes.size() == layout.total_size);

  auto read_from_bytes = [&baked_bytes]<typename T>(size_t offset) {
    T value;
    std::memcpy(&value, baked_bytes.data() + offset, sizeof(T));
    return value;
  };

  auto out_albedo = read_from_bytes.operator()<glm::vec3>(0);
  CHECK(out_albedo.x == doctest::Approx(1.0f));
  CHECK(out_albedo.y == doctest::Approx(0.5f));
  CHECK(out_albedo.z == doctest::Approx(0.25f));

  auto out_roughness = read_from_bytes.operator()<float>(12);
  CHECK(out_roughness == doctest::Approx(0.8f));

  auto out_texture_index = read_from_bytes.operator()<uint32_t>(16);
  CHECK(out_texture_index == expected_texture_index);
}

TEST_CASE("MaterialPacker handles missing properties safely") {
  // Setup dependencies properly using RenderingContext
  BindlessTextureRegistry registry;
  RenderingContext context;
  context.bindless_texture_registry = &registry; // Link it up

  MaterialPacker baker(context); // Pass the context, not the registry

  MaterialShaderLayout layout;
  layout.properties["missing_color"] = {0, 16};   // vec4
  layout.total_size = 16;

  Material material;  // Empty material

  PackedMaterial packed_material = baker.pack(material, layout);
  std::vector<std::byte> baked_bytes = packed_material.data;

  REQUIRE(baked_bytes.size() == 16);

  uint32_t zero_check = 0;
  std::memcpy(&zero_check, baked_bytes.data(), sizeof(uint32_t));
  CHECK(zero_check == 0);
}

} // mole
