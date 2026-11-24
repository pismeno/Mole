#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace mole {

struct CombinedShaderKey {
  uint64_t vert_id;
  uint64_t frag_id;

  bool operator==(const CombinedShaderKey& other) const {
    return vert_id == other.vert_id && frag_id == other.frag_id;
  }

  bool operator<(const CombinedShaderKey& other) const {
    if (vert_id != other.vert_id) return vert_id < other.vert_id;
    return frag_id < other.frag_id;
  }
};

struct CombinedShaderKeyHash {
  std::size_t operator()(const CombinedShaderKey& k) const {
    return std::hash<uint64_t>()(k.vert_id) ^ (std::hash<uint64_t>()(k.frag_id) << 1);
  }
};

/** @brief Identifies a pipeline: the shader pair plus the color attachment format it renders into. */
struct PipelineKey {
  CombinedShaderKey shader_key;
  uint32_t color_format; // vk::Format

  bool operator==(const PipelineKey& other) const {
    return shader_key == other.shader_key && color_format == other.color_format;
  }
};

struct PipelineKeyHash {
  std::size_t operator()(const PipelineKey& k) const {
    return CombinedShaderKeyHash()(k.shader_key) ^ (std::hash<uint32_t>()(k.color_format) << 2);
  }
};

struct PipelineShadersVersion {
  uint32_t vert_version;
  uint32_t frag_version;

  bool operator==(const PipelineShadersVersion& other) const {
    return vert_version == other.vert_version && frag_version == other.frag_version;
  }

  bool operator<(const PipelineShadersVersion& other) const {
    if (vert_version != other.vert_version) return vert_version < other.vert_version;
    return frag_version < other.frag_version;
  }
};

} // mole
