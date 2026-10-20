#include <doctest/doctest.h>

#include "osal/ifile_system.hpp"
#include <filesystem>
#include <fstream>

using namespace mole;

TEST_CASE("WindowsFileSystem: exists, is_file, and is_dir") {
  auto r_fs = IFileSystem::create();
  REQUIRE(r_fs.is_ok());
  auto fs = std::move(r_fs).value();

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "mole_fs_test";
  std::filesystem::create_directories(temp_dir);

  std::filesystem::path test_file = temp_dir / "test_file.txt";
  std::filesystem::path test_subdir = temp_dir / "subdir";
  std::filesystem::create_directories(test_subdir);

  {
    std::ofstream ofs(test_file);
    ofs << "hello mole";
  }

  fs->mount("test", temp_dir);

  SUBCASE("exists checks") {
    auto r_file_exists = fs->exists("test://test_file.txt");
    REQUIRE(r_file_exists.is_ok());
    CHECK(r_file_exists.value() == true);

    auto r_dir_exists = fs->exists("test://subdir");
    REQUIRE(r_dir_exists.is_ok());
    CHECK(r_dir_exists.value() == true);

    auto r_missing_exists = fs->exists("test://nonexistent.txt");
    REQUIRE(r_missing_exists.is_ok());
    CHECK(r_missing_exists.value() == false);

    auto r_invalid_prefix = fs->exists("unmounted://test_file.txt");
    CHECK_FALSE(r_invalid_prefix.is_ok());
  }

  SUBCASE("is_file checks") {
    auto r_is_file = fs->is_file("test://test_file.txt");
    REQUIRE(r_is_file.is_ok());
    CHECK(r_is_file.value() == true);

    auto r_dir_is_file = fs->is_file("test://subdir");
    REQUIRE(r_dir_is_file.is_ok());
    CHECK(r_dir_is_file.value() == false);

    auto r_missing_is_file = fs->is_file("test://nonexistent.txt");
    REQUIRE(r_missing_is_file.is_ok());
    CHECK(r_missing_is_file.value() == false);

    auto r_invalid_prefix = fs->is_file("unmounted://test_file.txt");
    CHECK_FALSE(r_invalid_prefix.is_ok());
  }

  SUBCASE("is_dir checks") {
    auto r_dir_is_dir = fs->is_dir("test://subdir");
    REQUIRE(r_dir_is_dir.is_ok());
    CHECK(r_dir_is_dir.value() == true);

    auto r_file_is_dir = fs->is_dir("test://test_file.txt");
    REQUIRE(r_file_is_dir.is_ok());
    CHECK(r_file_is_dir.value() == false);

    auto r_missing_is_dir = fs->is_dir("test://nonexistent_dir");
    REQUIRE(r_missing_is_dir.is_ok());
    CHECK(r_missing_is_dir.value() == false);

    auto r_invalid_prefix = fs->is_dir("unmounted://subdir");
    CHECK_FALSE(r_invalid_prefix.is_ok());
  }

  // Cleanup
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
}

