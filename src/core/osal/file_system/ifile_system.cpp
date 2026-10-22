#include "osal/ifile_system.hpp"

#include <memory>
#include <utility>

#include "utils/result.hpp"

#ifdef BIRD_PLATFORM_WINDOWS
#include "osal/file_system/windows_file_system.hpp"
#endif

namespace mole {

Result<std::unique_ptr<IFileSystem>> IFileSystem::create() {
#ifdef BIRD_PLATFORM_WINDOWS
  return std::unique_ptr<IFileSystem>(new WindowsFileSystem());
#else
  return mole::fail("Unsupported platform");
#endif
}

} // mole
