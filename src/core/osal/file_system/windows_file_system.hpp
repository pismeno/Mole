#pragma once

#include <osal/ifile_system.hpp>

#include <string_view>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace mole {

class WindowsFileSystem : public IFileSystem {

  friend class IFileSystem;

 public:
  ~WindowsFileSystem() override = default;

  void mount(std::string_view prefix, const std::filesystem::path& physical_path) override;
  Result<std::filesystem::path> resolve(std::string_view virtual_path) const override;

  Result<std::vector<uint8_t>> read_bytes(std::string_view path) override;
  Result<void> write_bytes(std::string_view path, const std::vector<uint8_t>& data) override;
  Result<std::vector<uint8_t>> read_chunk(std::string_view path, uint64_t offset, uint64_t size) override;
  Result<void> remove(std::string_view virtual_path) override;
  Result<void> make_file(std::string_view virtual_path) override;
  Result<bool> exists(std::string_view virtual_path) const override;
  Result<bool> is_file(std::string_view virtual_path) const override;
  Result<bool> is_dir(std::string_view virtual_path) const override;
  Result<uint64_t> get_file_size(std::string_view path) const override;
 private:
  WindowsFileSystem() = default;

  std::unordered_map<std::string, std::filesystem::path> mount_points;
};

} // mole
