#include "resources/asset_pool.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stb_image.h>

#include "osal/ifile_system.hpp"
#include "resources/asset_types.hpp"
#include "utils/result.hpp"
#include "utils/uuid.hpp"

namespace mole {

Result<std::vector<uint8_t>> read_asset_payload(IFileSystem* file_system, UUID uuid) {
  if (!file_system) {
    return mole::fail("File system not available");
  }

  std::string filepath = "assets://" + uuid.to_string() + ".asset";

  auto r_file_size = file_system->get_file_size(filepath);
  if (!r_file_size) {
    return mole::fail(r_file_size.error());
  }

  uint64_t header_size = sizeof(AssetHeader);
  if (r_file_size.value() < header_size) {
    return mole::fail("File smaller than asset header: " + filepath);
  }
  uint64_t payload_size = r_file_size.value() - header_size;

  auto r_read_bytes = file_system->read_chunk(filepath, header_size, payload_size);
  if (!r_read_bytes) {
    return mole::fail(r_read_bytes.error());
  }

  return r_read_bytes;
}

template <>
Result<ImageAsset> AssetPool<ImageAsset>::parse_asset(std::string_view filepath, std::vector<uint8_t> raw_bytes) {
  int w, h, c;

  stbi_uc* raw_pixels = stbi_load_from_memory(
      raw_bytes.data(),
      static_cast<int>(raw_bytes.size()),
      &w, &h, &c,
      STBI_rgb_alpha
  );

  if (!raw_pixels) {
    return mole::fail("Failed to decode image data");
  }

  ImageAsset asset;
  asset.width = static_cast<uint32_t>(w);
  asset.height = static_cast<uint32_t>(h);
  asset.channels = 4;

  size_t image_size = w * h * 4;
  asset.pixel_data.assign(raw_pixels, raw_pixels + image_size);

  stbi_image_free(raw_pixels);

  return asset;
}

} // mole
