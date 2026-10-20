#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>

#include <utils/result.hpp>

namespace mole {

class IFileSystem {
 public:
  explicit IFileSystem() = default;
  virtual ~IFileSystem() = default;

  /**
   * @brief Mounts a prefix for usage in virtual paths. It can be then used as "prefix://"
   * @param prefix The prefix to mount
   * @param physical_path The physical path to the directory it should be translated to
   */
  virtual void mount(std::string_view prefix, const std::filesystem::path& physical_path) = 0;

  /**
   * @brief Resolves virtual path starting with "mounted_prefix://".
   * This doesn't check if the path actually exists, just translates virtual path to std::filesystem::path
   * @return Resolved path for the given virtual path.
   */
  virtual Result<std::filesystem::path> resolve(std::string_view virtual_path) const = 0;

  /**
   * @brief Reads the bytes from the given path.
   * @return The read bytes, failed Result if the file could not be read from.
   */
  virtual Result<std::vector<uint8_t>> read_bytes(std::string_view virtual_path) = 0;

  /**
   * @brief Writes the given bytes to the given path.
   * @return Result, failed if the file could not be written to.
   */
  virtual Result<void> write_bytes(std::string_view virtual_path, const std::vector<uint8_t>& data) = 0;

  /**
 * @brief Reads a specific chunk of bytes from the file.
 * @param offset Byte offset from the start of the file.
 * @param size Number of bytes to read.
 * @return Result, failed if the file could not be written to.
 */
  virtual Result<std::vector<uint8_t>> read_chunk(std::string_view virtual_path, uint64_t offset, uint64_t size) = 0;

  virtual Result<bool> exists(std::string_view virtual_path) const = 0;
  virtual Result<bool> is_file(std::string_view virtual_path) const = 0;
  virtual Result<bool> is_dir(std::string_view virtual_path) const = 0;

  /**
   * @return The size of the file in bytes. Failed Result if the file could not be read from.
   */
  virtual Result<uint64_t> get_file_size(std::string_view virtual_path) const = 0;

  /**
   * @brief Factory method to create the specific implementation.
   */
  static Result<std::unique_ptr<IFileSystem>> create();
};

} // mole