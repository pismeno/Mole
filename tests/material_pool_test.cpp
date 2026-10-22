#include <cstdint>

#include <doctest/doctest.h>

#include "resources/material.hpp"
#include "resources/material_pool.hpp"
#include "resources/material_types.hpp"

using namespace mole;

TEST_CASE("MaterialPool: Basic Add and Validity") {
  MaterialPool pool;
  Material mat;

  MaterialID id = pool.add("mat_base", mat);

  CHECK(id != MaterialID::INVALID);
  CHECK(pool.is_valid(id));
  CHECK(pool.is_valid("mat_base"));
  CHECK(pool.get_id("mat_base") == id);
}

TEST_CASE("MaterialPool: Version increases on set_property") {
  MaterialPool pool;
  Material mat;
  MaterialID id = pool.add("mat_version_test", mat);

  uint32_t initial_version = pool.get_version(id);

  SUBCASE("set_property by ID increments version") {
    pool.set_property(id, "albedo", 1.0f);
    uint32_t new_version = pool.get_version(id);

    CHECK(new_version > initial_version);
    CHECK(new_version == initial_version + 1);

    // Calling it again should increment it again
    pool.set_property(id, "roughness", 0.5f);
    CHECK(pool.get_version(id) == new_version + 1);
  }

  SUBCASE("set_property by string ID increments version") {
    pool.set_property("mat_version_test", "metallic", 1.0f);
    uint32_t new_version = pool.get_version("mat_version_test");

    CHECK(new_version > initial_version);
    CHECK(new_version == initial_version + 1);

    // Verify the ID lookup matches the string lookup
    CHECK(pool.get_version(id) == new_version);
  }
}

TEST_CASE("MaterialPool: Generations behave correctly on destroy/reuse") {
  MaterialPool pool;
  Material mat;

  MaterialID id1 = pool.add("mat_gen_1", mat);
  CHECK(pool.is_valid(id1));

  pool.destroy(id1);

  // The old ID should now be strictly invalid
  CHECK_FALSE(pool.is_valid(id1));
  CHECK_FALSE(pool.is_valid("mat_gen_1"));

  // Adding a new material should reuse the slot but bump the generation
  MaterialID id2 = pool.add("mat_gen_2", mat);
  CHECK(pool.is_valid(id2));

  // Ensure the pool reuses the same index (standard generational arena behavior)
  CHECK(id1.index() == id2.index());

  // Ensure the generation has been incremented to prevent ABA problems
  CHECK(id2.generation() > id1.generation());

  // Double check that querying the old ID still returns invalid,
  // even though the underlying index is currently in use by id2
  CHECK_FALSE(pool.is_valid(id1));
}

TEST_CASE("MaterialPool: Destroying by String ID updates generations") {
  MaterialPool pool;
  Material mat;

  MaterialID id1 = pool.add("mat_str_destroy", mat);
  pool.destroy("mat_str_destroy");

  CHECK_FALSE(pool.is_valid(id1));

  MaterialID id2 = pool.add("mat_new", mat);
  CHECK(id1.index() == id2.index());
  CHECK(id2.generation() > id1.generation());
}

TEST_CASE("MaterialPool: clear() invalidates all existing IDs") {
  MaterialPool pool;
  Material mat;

  MaterialID id1 = pool.add("mat1", mat);
  MaterialID id2 = pool.add("mat2", mat);

  CHECK(pool.is_valid(id1));
  CHECK(pool.is_valid(id2));

  pool.clear();

  CHECK_FALSE(pool.is_valid(id1));
  CHECK_FALSE(pool.is_valid(id2));
  CHECK_FALSE(pool.is_valid("mat1"));
  CHECK_FALSE(pool.is_valid("mat2"));

  // After clear, if we add new materials, the generations of the reused indices should still bump
  MaterialID id3 = pool.add("mat3", mat);

  // Assuming clear() resets the free-list, id3 might reuse id1's index
  if (id3.index() == id1.index()) {
    CHECK(id3.generation() > id1.generation());
  } else if (id3.index() == id2.index()) {
    CHECK(id3.generation() > id2.generation());
  }
}
