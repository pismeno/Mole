#include <optional>
#include <vector>

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

TEST_CASE("SlotMap: clear invalidates all IDs") {
  SlotMap<TestID, int> map;
  TestID a = map.insert(1);
  TestID b = map.insert(2);
  map.clear();

  CHECK_FALSE(map.contains(a));
  CHECK_FALSE(map.contains(b));
  CHECK(map.size() == 0);

  TestID c = map.insert(3);
  CHECK(c.index() == a.index());
  CHECK(c.generation() != a.generation());
  CHECK_FALSE(map.contains(a));
}

TEST_CASE("SlotMap: for_each skips dead slots") {
  SlotMap<TestID, int> map;
  TestID a = map.insert(1);
  TestID b = map.insert(2);
  TestID c = map.insert(3);
  (void)map.erase(b);

  std::vector<TestID> visited;
  int sum = 0;
  map.for_each([&](TestID id, int& value) {
    visited.push_back(id);
    sum += value;
  });

  REQUIRE(visited.size() == 2);
  CHECK(visited[0] == a);
  CHECK(visited[1] == c);
  CHECK(sum == 4);
}

TEST_CASE("SlotMap: capacity limit") {
  SlotMap<TestID, int> map{2};
  TestID a = map.insert(1);
  (void)map.insert(2);
  CHECK(map.insert(3) == TestID::INVALID);

  (void)map.erase(a);
  CHECK(map.insert(4) != TestID::INVALID);
  CHECK(map.insert(5) == TestID::INVALID);
}

TEST_CASE("SlotMap: size and slot_count") {
  SlotMap<TestID, int> map;
  TestID a = map.insert(1);
  (void)map.insert(2);
  CHECK(map.size() == 2);
  CHECK(map.slot_count() == 2);

  (void)map.erase(a);
  CHECK(map.size() == 1);
  CHECK(map.slot_count() == 2);
}
