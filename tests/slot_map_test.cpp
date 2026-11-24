#include <optional>

#include <doctest/doctest.h>

#include "utils/id.hpp"
#include "utils/slot_map.hpp"

using namespace mole;

using TestID = ID<struct TestIDTag>;

TEST_CASE("SlotMap: insert and get") {
  SlotMap<TestID, int> map;
  TestID a = map.insert(1);
  TestID b = map.insert(2);

  REQUIRE(map.contains(a));
  REQUIRE(map.contains(b));
  CHECK(*map.get(a) == 1);
  CHECK(*map.get(b) == 2);
}

TEST_CASE("SlotMap: erase returns the value") {
  SlotMap<TestID, int> map;
  TestID a = map.insert(42);

  std::optional<int> erased = map.erase(a);
  REQUIRE(erased.has_value());
  CHECK(*erased == 42);
  CHECK_FALSE(map.contains(a));
  CHECK(map.get(a) == nullptr);
  CHECK_FALSE(map.erase(a).has_value());
}

TEST_CASE("SlotMap: stale ID after erase and reuse") {
  SlotMap<TestID, int> map;
  TestID a = map.insert(1);
  (void)map.erase(a);
  TestID b = map.insert(2);

  CHECK(b.index() == a.index());
  CHECK(b.generation() != a.generation());
  CHECK(map.get(a) == nullptr);
  REQUIRE(map.get(b) != nullptr);
  CHECK(*map.get(b) == 2);
}

TEST_CASE("SlotMap: INVALID is never contained") {
  SlotMap<TestID, int> map;
  (void)map.insert(1);

  CHECK_FALSE(map.contains(TestID::INVALID));
  CHECK(map.get(TestID::INVALID) == nullptr);
}
