#include <cstdint>

#include <doctest/doctest.h>
#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

#include "node_system/managers/transform_manager.hpp"
#include "node_system/node_types.hpp"

using namespace mole;

inline void CHECK_TRANSLATION(const glm::mat3x3& mat, float expected_x, float expected_y) {
  CHECK(mat[2][0] == doctest::Approx(expected_x));
  CHECK(mat[2][1] == doctest::Approx(expected_y));
}

inline void CHECK_SCALE(const glm::mat3x3& mat, float expected_x, float expected_y) {
  CHECK(mat[0][0] == doctest::Approx(expected_x));
  CHECK(mat[1][1] == doctest::Approx(expected_y));
}

TEST_CASE("TransformManager - Initialization and Lifecycle") {
  TransformManager tm;
  NodeID node = NodeID::from(1, 0);

  SUBCASE("Nodes start with no transform") {
    CHECK_FALSE(tm.has_transform(node));
  }

  SUBCASE("Requiring manager initializes an identity transform") {
    tm.on_node_require_manager(node);
    CHECK(tm.has_transform(node));

    const auto& mat = tm.get_global_matrix(node);
    CHECK_TRANSLATION(mat, 0.0f, 0.0f);
    CHECK_SCALE(mat, 1.0f, 1.0f);
  }

  SUBCASE("Destroying a node cleans up its transform") {
    tm.on_node_require_manager(node);
    tm.on_node_destroyed(node);
    CHECK_FALSE(tm.has_transform(node));
  }
}

TEST_CASE("TransformManager - Local Transformations") {
  TransformManager tm;
  NodeID node = NodeID::from(2, 1);

  tm.on_node_require_manager(node);

  SUBCASE("Position") {
    REQUIRE(tm.set_node_position(node, glm::vec2(15.5f, -10.0f)).is_ok());
    tm.on_update(); // Flush/calculate matrices

    CHECK_TRANSLATION(tm.get_global_matrix(node), 15.5f, -10.0f);
  }

  SUBCASE("Scale") {
    REQUIRE(tm.set_node_scale(node, glm::vec2(2.0f, 4.0f)).is_ok());
    tm.on_update();

    CHECK_SCALE(tm.get_global_matrix(node), 2.0f, 4.0f);
  }
}

TEST_CASE("TransformManager - Logical to Spatial Hierarchy") {
  TransformManager tm;

  NodeID parent = NodeID::from(1, 0);
  NodeID child = NodeID::from(2, 0);
  NodeID grandchild = NodeID::from(3, 0);

  tm.on_node_require_manager(parent);
  tm.on_node_require_manager(child);
  tm.on_node_require_manager(grandchild);

  SUBCASE("Translation is inherited additively via reparenting hook") {
    REQUIRE(tm.set_node_position(parent, glm::vec2(10.0f, 10.0f)).is_ok());
    REQUIRE(tm.set_node_position(child, glm::vec2(5.0f, -5.0f)).is_ok());

    // Link them using the logical hook
    tm.on_node_reparented(child, parent, NodeID::INVALID);
    tm.on_update();

    // Parent should be exactly where we put it
    CHECK_TRANSLATION(tm.get_global_matrix(parent), 10.0f, 10.0f);
    // Child's global should be Parent + Child (10+5, 10-5)
    CHECK_TRANSLATION(tm.get_global_matrix(child), 15.0f, 5.0f);
  }

  SUBCASE("Scale cascades down and scales child translation") {
    REQUIRE(tm.set_node_scale(parent, glm::vec2(2.0f, 2.0f)).is_ok());
    REQUIRE(tm.set_node_position(child, glm::vec2(10.0f, 0.0f)).is_ok());

    tm.on_node_reparented(child, parent, NodeID::INVALID);
    tm.on_update();

    // The child is 10 units to the right in LOCAL space.
    // Because the parent is scaled by 2, global translation should be 20.
    CHECK_TRANSLATION(tm.get_global_matrix(child), 20.0f, 0.0f);
    CHECK_SCALE(tm.get_global_matrix(child), 2.0f, 2.0f);
  }

  SUBCASE("Deep Hierarchy (Grandchild)") {
    REQUIRE(tm.set_node_position(parent, glm::vec2(100.0f, 0.0f)).is_ok());
    REQUIRE(tm.set_node_position(child, glm::vec2(50.0f, 0.0f)).is_ok());
    REQUIRE(tm.set_node_position(grandchild, glm::vec2(25.0f, 0.0f)).is_ok());

    // Build the logical tree
    tm.on_node_reparented(child, parent, NodeID::INVALID);
    tm.on_node_reparented(grandchild, child, NodeID::INVALID);
    tm.on_update();

    CHECK_TRANSLATION(tm.get_global_matrix(grandchild), 175.0f, 0.0f);
  }
}

TEST_CASE("TransformManager - Reparenting and Detaching") {
  TransformManager tm;
  NodeID old_parent = NodeID::from(1, 0);
  NodeID new_parent = NodeID::from(2, 0);
  NodeID child = NodeID::from(3, 0);

  tm.on_node_require_manager(old_parent);
  tm.on_node_require_manager(new_parent);
  tm.on_node_require_manager(child);

  REQUIRE(tm.set_node_position(old_parent, glm::vec2(10.0f, 10.0f)).is_ok());
  REQUIRE(tm.set_node_position(new_parent, glm::vec2(50.0f, 50.0f)).is_ok());
  REQUIRE(tm.set_node_position(child, glm::vec2(0.0f, 0.0f)).is_ok());

  SUBCASE("Detaching a logical parent resets child to local transform") {
    // Attach to old parent initially
    tm.on_node_reparented(child, old_parent, NodeID::INVALID);
    tm.on_update();
    CHECK_TRANSLATION(tm.get_global_matrix(child), 10.0f, 10.0f);

    // Reparent to nothing (detach)
    tm.on_node_reparented(child, NodeID::INVALID, old_parent);
    tm.on_update();
    CHECK_TRANSLATION(tm.get_global_matrix(child), 0.0f, 0.0f); // Back to local 0,0
  }

  SUBCASE("Switching logical parents updates global transform") {
    // Attach to old parent
    tm.on_node_reparented(child, old_parent, NodeID::INVALID);
    tm.on_update();

    // Switch from old_parent to new_parent
    tm.on_node_reparented(child, new_parent, old_parent);
    tm.on_update();
    CHECK_TRANSLATION(tm.get_global_matrix(child), 50.0f, 50.0f);
  }
}

TEST_CASE("TransformManager - Scene Clear") {
  TransformManager tm;
  NodeID node = NodeID::from(1, 0);

  tm.on_node_require_manager(node);
  CHECK(tm.has_transform(node));

  tm.on_scene_clear();

  CHECK_FALSE(tm.has_transform(node));
}

TEST_CASE("TransformManager - Nearest Spatial Parent Resolution") {
  TransformManager tm;

  NodeID root = NodeID::from(1, 0);
  NodeID logical_middle1 = NodeID::from(2, 0);
  NodeID logical_middle2 = NodeID::from(3, 0);
  NodeID leaf = NodeID::from(4, 0);

  // Only the root and the leaf get transforms initially.
  // The middle nodes act purely as logical organizers (e.g. empty folders/groups).
  tm.on_node_require_manager(root);
  tm.on_node_require_manager(leaf);

  REQUIRE(tm.set_node_position(root, glm::vec2(100.0f, 50.0f)).is_ok());
  REQUIRE(tm.set_node_position(leaf, glm::vec2(10.0f, 10.0f)).is_ok());

  SUBCASE("Leaf inherits transform from distant root through transform-less logical nodes") {
    // Build the logical chain: root -> middle1 -> middle2 -> leaf
    tm.on_node_reparented(logical_middle1, root, NodeID::INVALID);
    tm.on_node_reparented(logical_middle2, logical_middle1, NodeID::INVALID);
    tm.on_node_reparented(leaf, logical_middle2, NodeID::INVALID);

    tm.on_update();

    // Leaf's spatial parent should resolve to 'root', completely skipping middle1 and middle2.
    // Expected Global: Root(100, 50) + Leaf(10, 10) = (110, 60)
    CHECK_TRANSLATION(tm.get_global_matrix(leaf), 110.0f, 60.0f);
  }

  SUBCASE("Inserting a spatial node into the logical chain intercepts the spatial hierarchy") {
    // Build initial chain: root -> logical_middle1 -> leaf
    tm.on_node_reparented(logical_middle1, root, NodeID::INVALID);
    tm.on_node_reparented(leaf, logical_middle1, NodeID::INVALID);

    tm.on_update();
    CHECK_TRANSLATION(tm.get_global_matrix(leaf), 110.0f, 60.0f); // Inherits from root

    // Create a NEW node that already has a transform initialized correctly
    NodeID spatial_middle = NodeID::from(5, 0);
    tm.on_node_require_manager(spatial_middle);
    REQUIRE(tm.set_node_position(spatial_middle, glm::vec2(20.0f, 0.0f)).is_ok());

    // Insert it between root and logical_middle1
    // 1. Attach the new node to root
    tm.on_node_reparented(spatial_middle, root, NodeID::INVALID);
    // 2. Reparent logical_middle1 from root to spatial_middle
    tm.on_node_reparented(logical_middle1, spatial_middle, root);

    tm.on_update();

    // spatial_middle global: Root(100, 50) + SpatialMiddle(20, 0) = (120, 50)
    CHECK_TRANSLATION(tm.get_global_matrix(spatial_middle), 120.0f, 50.0f);

    // leaf global: SpatialMiddle_Global(120, 50) + Leaf(10, 10) = (130, 60)
    // This proves the leaf's spatial parent was successfully redirected to spatial_middle!
    CHECK_TRANSLATION(tm.get_global_matrix(leaf), 130.0f, 60.0f);
  }
}
