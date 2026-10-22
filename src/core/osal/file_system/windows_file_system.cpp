#include "osal/file_system/windows_file_system.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "utils/result.hpp"

namespace mole {

namespace {
// Internal helper for RAII handle management, hidden in anonymous namespace
class scoped_win32_handle {
 public:
  explicit scoped_win32_handle(HANDLE raw_handle) : handle(raw_handle) {}

  ~scoped_win32_handle() {
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
      CloseHandle(handle);
    }
  }

  HANDLE get() const { return handle; }
  bool is_valid() const { return handle != INVALID_HANDLE_VALUE; }

 private:
  HANDLE handle;
};

// Internal helper to convert engine UTF-8 strings to Windows UTF-16 strings
std::wstring utf8_to_utf16(std::string_view utf8_str) {
  if (utf8_str.empty()) return {};

  int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(),
                                        static_cast<int>(utf8_str.size()), nullptr, 0);
  std::wstring result(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(),
                      static_cast<int>(utf8_str.size()), result.data(), size_needed);

  return result;
}
} // anonymous namespace

Result<std::filesystem::path> WindowsFileSystem::resolve(std::string_view virtual_path) const {
  auto delim_pos = virtual_path.find("://");

  if (delim_pos == std::string_view::npos) {
    return std::filesystem::path(virtual_path);
  }

  std::string prefix = std::string(virtual_path.substr(0, delim_pos));
  std::string_view relative_path = virtual_path.substr(delim_pos + 3);

  if (auto it = mount_points.find(prefix); it != mount_points.end()) {
    return it->second / relative_path;
  }

  return mole::fail("No mount point for prefix: " + prefix);
}

void WindowsFileSystem::mount(std::string_view prefix, const std::filesystem::path& physical_path) {
  mount_points[std::string(prefix)] = std::filesystem::absolute(physical_path);
}

Result<std::vector<uint8_t>> WindowsFileSystem::read_bytes(std::string_view path) {
  auto resolved = resolve(path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());

  scoped_win32_handle file(CreateFileW(
      wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr
  ));

  if (!file.is_valid()) {
    return fail("Failed to open file for reading: " + resolved.value().string());
  }

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file.get(), &file_size)) {
    return fail("Failed to get file size: " + resolved.value().string());
  }

  if (file_size.QuadPart > UINT32_MAX) {
    return fail("File too large for single-pass read: " + resolved.value().string());
  }

  std::vector<uint8_t> buffer;
  buffer.resize(static_cast<size_t>(file_size.QuadPart));

  DWORD bytes_read = 0;
  if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr)) {
    return fail("Failed to read file contents: " + resolved.value().string());
  }

  if (bytes_read != buffer.size()) {
    return fail("Read fewer bytes than expected from: " + resolved.value().string());
  }

  return buffer;
}

Result<void> WindowsFileSystem::write_bytes(std::string_view path, const std::vector<uint8_t>& data) {
  auto resolved = resolve(path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());

  scoped_win32_handle file(CreateFileW(
      wpath.c_str(), GENERIC_WRITE, 0, nullptr,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
  ));

  if (!file.is_valid()) {
    return fail("Failed to open file for writing: " + resolved.value().string());
  }

  if (data.size() > UINT32_MAX) {
    return fail("Data too large for single-pass write: " + resolved.value().string());
  }

  DWORD bytes_written = 0;
  if (!WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &bytes_written, nullptr)) {
    return fail("Failed to write to file: " + resolved.value().string());
  }

  if (bytes_written != data.size()) {
    return fail("Wrote fewer bytes than expected to: " + resolved.value().string());
  }

  return ok();
}

Result<std::vector<uint8_t>> WindowsFileSystem::read_chunk(std::string_view path, uint64_t offset, uint64_t size) {
  auto resolved = resolve(path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());

  scoped_win32_handle file(CreateFileW(
      wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr
  ));

  if (!file.is_valid()) {
    return fail("Failed to open file for chunk reading: " + resolved.value().string());
  }

  // Windows ReadFile takes a DWORD (32-bit unsigned int) for the number of bytes to read
  if (size > UINT32_MAX) {
    return fail("Chunk size too large for single-pass read: " + resolved.value().string());
  }

  // Seek to the specified offset
  LARGE_INTEGER li_offset;
  li_offset.QuadPart = static_cast<LONGLONG>(offset);
  if (!SetFilePointerEx(file.get(), li_offset, nullptr, FILE_BEGIN)) {
    return fail("Failed to seek to offset in file: " + resolved.value().string());
  }

  std::vector<uint8_t> buffer;
  // If size is 0, we can return empty buffer early
  if (size == 0) {
    return buffer;
  }

  buffer.resize(static_cast<size_t>(size));

  DWORD bytes_read = 0;
  if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr)) {
    return fail("Failed to read chunk from file: " + resolved.value().string());
  }

  // If we read past the end of the file, shrink the buffer to the actual bytes read
  if (bytes_read != buffer.size()) {
    buffer.resize(bytes_read);
  }

  return buffer;
}

Result<void> WindowsFileSystem::remove(std::string_view virtual_path) {
  auto resolved = resolve(virtual_path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(virtual_path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());
  DWORD attributes = GetFileAttributesW(wpath.c_str());

  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return fail("Failed to remove, path does not exist: " + resolved.value().string());
  }

  if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
    if (!RemoveDirectoryW(wpath.c_str())) {
      return fail("Failed to remove directory: " + resolved.value().string());
    }
  } else {
    if (attributes & FILE_ATTRIBUTE_READONLY) {
      SetFileAttributesW(wpath.c_str(), attributes & ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY));
    }
    if (!DeleteFileW(wpath.c_str())) {
      return fail("Failed to delete file: " + resolved.value().string());
    }
  }

  return ok();
}

Result<void> WindowsFileSystem::make_file(std::string_view virtual_path) {
  auto resolved = resolve(virtual_path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(virtual_path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());

  scoped_win32_handle file(CreateFileW(
      wpath.c_str(), GENERIC_WRITE, 0, nullptr,
      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
  ));

  if (!file.is_valid()) {
    return fail("Failed to create file: " + resolved.value().string());
  }

  return ok();
}

Result<bool> WindowsFileSystem::exists(std::string_view virtual_path) const {
  auto resolved = resolve(virtual_path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(virtual_path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());
  DWORD attributes = GetFileAttributesW(wpath.c_str());

  return (attributes != INVALID_FILE_ATTRIBUTES);
}

Result<bool> WindowsFileSystem::is_file(std::string_view virtual_path) const {
  auto resolved = resolve(virtual_path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(virtual_path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());
  DWORD attributes = GetFileAttributesW(wpath.c_str());

  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }

  return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

Result<bool> WindowsFileSystem::is_dir(std::string_view virtual_path) const {
  auto resolved = resolve(virtual_path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(virtual_path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());
  DWORD attributes = GetFileAttributesW(wpath.c_str());

  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }

  return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

Result<uint64_t> WindowsFileSystem::get_file_size(std::string_view path) const {
  auto resolved = resolve(path);
  if (!resolved) {
    return fail("Failed to resolve virtual path: " + std::string(path));
  }

  std::wstring wpath = utf8_to_utf16(resolved.value().string());
  WIN32_FILE_ATTRIBUTE_DATA file_info;

  if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &file_info)) {
    return fail("Failed to get file attributes: " + resolved.value().string());
  }

  LARGE_INTEGER size;
  size.HighPart = file_info.nFileSizeHigh;
  size.LowPart = file_info.nFileSizeLow;

  return static_cast<uint64_t>(size.QuadPart);
}

} // mole
