#include "rendering/material_shader_layout_reflector.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include <spirv_cross.hpp>

#include "rendering/material_shader_layout.hpp"
#include "utils/result.hpp"

namespace mole {

Result<MaterialShaderLayout> MaterialShaderLayoutReflector::reflect(const uint32_t* spirv_words, size_t size_bytes) const {
  MaterialShaderLayout layout_info;

  // Compiler expects the word count, so divide size_bytes by sizeof(uint32_t)
  spirv_cross::Compiler compiler(spirv_words, size_bytes / sizeof(uint32_t));

  // Extract all resources (UBOs, SSBOs, Push Constants, etc.)
  spirv_cross::ShaderResources resources = compiler.get_shader_resources();

  bool found_buffer = false;

  // Find the Material SSBO
  for (const auto& resource : resources.storage_buffers) {

    // Match the name of your SSBO block in GLSL
    if (resource.name != "MaterialBuffer") {
      continue;
    }

    const spirv_cross::SPIRType& block_type = compiler.get_type(resource.base_type_id);

    // In bindless, the SSBO usually contains a single runtime array (e.g., MaterialData materials[])
    if (block_type.member_types.empty()) continue; // It doesnt contain MaterialData array, we continue

    const spirv_cross::SPIRType& array_type = compiler.get_type(block_type.member_types[0]);
    const spirv_cross::SPIRType& struct_type = compiler.get_type(array_type.parent_type);
    size_t member_count = struct_type.member_types.size();

    for (uint32_t i = 0; i < member_count; i++) {
      std::string member_name = compiler.get_member_name(struct_type.self, i);

      // Get the exact memory offset defined by std430 rules
      uint32_t offset = compiler.type_struct_member_offset(struct_type, i);

      // Get the size of the variable in bytes
      uint32_t size = compiler.get_declared_struct_member_size(struct_type, i);

      layout_info.properties[member_name] = {offset, size};
    }

    // Get the total size of the struct, including any std430 padding at the end
    layout_info.total_size = compiler.get_declared_struct_size(struct_type);

    found_buffer = true;
    break;
  }

  if (!found_buffer) {
    return mole::fail("Missing 'MaterialBuffer' SSBO");
  }

  return layout_info;
}

} // mole
