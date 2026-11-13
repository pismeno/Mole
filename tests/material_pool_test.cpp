#include <cstdint>

#include <doctest/doctest.h>

#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/material_types.hpp"
#include "resources/resources_context.hpp"

using namespace mole;

TEST_CASE("MaterialPool: Basic Add and Validity") {
  ResourcesContext res_ctx;
  MaterialPool pool(res_ctx);
  Material mat;

  MaterialID id = pool.add(std::move(mat));

  CHECK(id != MaterialID::INVALID);
  CHECK(pool.is_valid(id));
  CHECK_FALSE(pool.is_valid(MaterialID::INVALID));
  CHECK_FALSE(pool.is_valid(MaterialID::from(999, 0)));
}

TEST_CASE("MaterialPool: Value accessors and scoped callbacks") {
  ResourcesContext res_ctx;
  MaterialPool pool(res_ctx);
  Material mat;
  mat.properties["roughness"] = 0.5f;
  mat.properties["albedo_tint"] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

  MaterialID id = pool.add(std::move(mat));
  REQUIRE(pool.is_valid(id));

  SUBCASE("get_property returns property value or fails") {
    auto r_prop = pool.get_property(id, "roughness");
    REQUIRE(r_prop.is_ok());
    CHECK(std::get<float>(r_prop.value()) == 0.5f);

    auto r_vec = pool.get_property(id, "albedo_tint");
    REQUIRE(r_vec.is_ok());
    CHECK(std::get<glm::vec4>(r_vec.value()) == glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    auto r_missing = pool.get_property(id, "non_existent");
    CHECK_FALSE(r_missing.is_ok());

    auto r_invalid = pool.get_property(MaterialID::INVALID, "roughness");
    CHECK_FALSE(r_invalid.is_ok());
  }

  SUBCASE("with_material allows safe inspection") {
    bool inspected = false;
    auto r_with = pool.with_material(id, [&](const Material& m) {
      inspected = true;
      CHECK(m.properties.size() == 2);
    });
    REQUIRE(r_with.is_ok());
    CHECK(inspected);

    auto r_with_invalid = pool.with_material(MaterialID::INVALID, [&](const Material&) {});
    CHECK_FALSE(r_with_invalid.is_ok());
  }
}

TEST_CASE("MaterialPool: Version increases on set_property and mutators") {
  ResourcesContext res_ctx;
  MaterialPool pool(res_ctx);
  Material mat;
  MaterialID id = pool.add(std::move(mat));

  auto r_initial_version = pool.get_version(id);
  REQUIRE(r_initial_version.is_ok());
  uint32_t initial_version = r_initial_version.value();

  SUBCASE("set_property by ID increments version") {
    REQUIRE(pool.set_property(id, "albedo", 1.0f).is_ok());
    auto r_new_version = pool.get_version(id);
    REQUIRE(r_new_version.is_ok());
    uint32_t new_version = r_new_version.value();

    CHECK(new_version > initial_version);
    CHECK(new_version == initial_version + 1);

    // Calling it again should increment it again
    REQUIRE(pool.set_property(id, "roughness", 0.5f).is_ok());
    CHECK(pool.get_version(id).value() == new_version + 1);

    // Verify properties were stored
    auto r_albedo = pool.get_property(id, "albedo");
    REQUIRE(r_albedo.is_ok());
    CHECK(std::get<float>(r_albedo.value()) == 1.0f);

    auto r_rough = pool.get_property(id, "roughness");
    REQUIRE(r_rough.is_ok());
    CHECK(std::get<float>(r_rough.value()) == 0.5f);
  }

  SUBCASE("set_vertex_shader and set_fragment_shader increment version") {
    uint32_t v_before = pool.get_version(id).value();
    REQUIRE(pool.set_vertex_shader(id, AssetHandle<VertexShaderAsset>{}).is_ok());
    CHECK(pool.get_version(id).value() == v_before + 1);

    v_before = pool.get_version(id).value();
    REQUIRE(pool.set_fragment_shader(id, AssetHandle<FragmentShaderAsset>{}).is_ok());
    CHECK(pool.get_version(id).value() == v_before + 1);
  }
}

TEST_CASE("MaterialPool: Generations behave correctly on destroy/reuse") {
  ResourcesContext res_ctx;
  MaterialPool pool(res_ctx);
  Material mat;

  MaterialID id1 = pool.add(std::move(mat));
  CHECK(pool.is_valid(id1));

  REQUIRE(pool.destroy(id1).is_ok());

  // The old ID should now be strictly invalid
  CHECK_FALSE(pool.is_valid(id1));

  // Destroying already destroyed material fails
  CHECK_FALSE(pool.destroy(id1).is_ok());

  // Adding a new material should reuse the slot but bump the generation
  MaterialID id2 = pool.add(Material{});
  CHECK(pool.is_valid(id2));

  // Ensure the pool reuses the same index (standard generational arena behavior)
  CHECK(id1.index() == id2.index());

  // Ensure the generation has been incremented to prevent ABA problems
  CHECK(id2.generation() > id1.generation());

  // Double check that querying the old ID still returns invalid,
  // even though the underlying index is currently in use by id2
  CHECK_FALSE(pool.is_valid(id1));
}

TEST_CASE("MaterialPool: clear() invalidates all existing IDs") {
  ResourcesContext res_ctx;
  MaterialPool pool(res_ctx);

  MaterialID id1 = pool.add(Material{});
  MaterialID id2 = pool.add(Material{});

  CHECK(pool.is_valid(id1));
  CHECK(pool.is_valid(id2));

  pool.clear();

  CHECK_FALSE(pool.is_valid(id1));
  CHECK_FALSE(pool.is_valid(id2));

  // After clear, if we add new materials, the generations of the reused indices should still bump
  MaterialID id3 = pool.add(Material{});

  if (id3.index() == id1.index()) {
    CHECK(id3.generation() > id1.generation());
  } else if (id3.index() == id2.index()) {
    CHECK(id3.generation() > id2.generation());
  }
}
