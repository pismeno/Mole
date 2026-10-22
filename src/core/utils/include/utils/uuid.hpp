#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <random>
#include <string>

namespace mole {

struct UUID { // Implemented using Gemini AI
  std::array<uint8_t, 16> data{};

  // Default constructor (creates an empty/zero UUID)
  UUID() = default;

  // Construct from existing data
  explicit UUID(const std::array<uint8_t, 16>& d) : data(d) {}

  // Generates a Python-compatible uuid.uuid4()
  static UUID generate_v4() {
    // Use thread_local to avoid locking overhead on random generation
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    UUID id;

    // Fill the 16 bytes using two 64-bit random numbers
    uint64_t* p = reinterpret_cast<uint64_t*>(id.data.data());
    p[0] = dis(gen);
    p[1] = dis(gen);

    // Apply RFC 4122 Version 4 bitmasks
    id.data[6] = (id.data[6] & 0x0F) | 0x40; // Version 4
    id.data[8] = (id.data[8] & 0x3F) | 0x80; // Variant 1

    return id;
  }

  // Matches Python's str(uuid.uuid4()) format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  std::string to_string() const {
    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             data[0], data[1], data[2], data[3],
             data[4], data[5],
             data[6], data[7],
             data[8], data[9],
             data[10], data[11], data[12], data[13], data[14], data[15]);
    return std::string(buf);
  }

  static UUID from_string(const std::string& str) {
    UUID id;

    // A standard UUID string is exactly 36 characters long
    if (str.length() != 36) {
      return id; // Or throw std::invalid_argument("Invalid UUID length")
    }

    // Use an intermediate array of unsigned int because %x expects a 4-byte integer.
    // Reading directly into uint8_t using %x causes memory corruption.
    unsigned int bytes[16];

    int matched = sscanf(str.c_str(),
                         "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                         &bytes[0], &bytes[1], &bytes[2], &bytes[3],
                         &bytes[4], &bytes[5],
                         &bytes[6], &bytes[7],
                         &bytes[8], &bytes[9],
                         &bytes[10], &bytes[11], &bytes[12], &bytes[13], &bytes[14], &bytes[15]);

    if (matched == 16) {
      for (int i = 0; i < 16; ++i) {
        id.data[i] = static_cast<uint8_t>(bytes[i]);
      }
    } else {
      // Handle error if string is malformed
      // throw std::invalid_argument("Malformed UUID string");
    }

    return id;
  }

  // Equality operators (required for maps/comparisons)
  bool operator==(const UUID& other) const { return data == other.data; }
  bool operator!=(const UUID& other) const { return data != other.data; }
};

} // mole

// std::hash specialization so you can use UUID in std::unordered_map<UUID, AssetHandle<T>>
template <>
struct std::hash<mole::UUID> {
  std::size_t operator()(const mole::UUID& uuid) const noexcept {
    // Fast hash by combining the two 64-bit halves
    const uint64_t* p = reinterpret_cast<const uint64_t*>(uuid.data.data());
    return std::hash<uint64_t>()(p[0]) ^ (std::hash<uint64_t>()(p[1]) << 1);
  }
};
