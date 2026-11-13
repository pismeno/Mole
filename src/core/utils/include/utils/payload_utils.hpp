#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace mole {

enum class EPayloadFormat {
  Json,
  Binary,
};

inline EPayloadFormat detect_payload_format(std::span<const uint8_t> payload) {
  size_t i = 0;
  // Skip UTF-8 BOM if present
  if (payload.size() >= 3 && payload[0] == 0xEF && payload[1] == 0xBB && payload[2] == 0xBF) {
    i = 3;
  }
  for (; i < payload.size(); ++i) {
    uint8_t byte = payload[i];
    if (byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n') {
      continue;
    }
    if (byte == '{' || byte == '[') {
      return EPayloadFormat::Json;
    }
    return EPayloadFormat::Binary;
  }
  return EPayloadFormat::Binary;
}

inline bool is_json_payload(std::span<const uint8_t> payload) {
  return detect_payload_format(payload) == EPayloadFormat::Json;
}

inline bool is_json_payload(const std::vector<uint8_t>& payload) {
  return is_json_payload(std::span<const uint8_t>(payload));
}

inline bool is_json_payload(std::string_view payload) {
  return is_json_payload(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(payload.data()), payload.size()));
}

} // mole
