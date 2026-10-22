if (APPLE)
    add_compile_definitions(BIRD_PLATFORM_APPLE)
elseif (UNIX AND NOT APPLE)
    add_compile_definitions(BIRD_PLATFORM_LINUX)
elseif (WIN32)
    add_compile_definitions(BIRD_PLATFORM_WINDOWS)
endif()

add_compile_definitions(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=1)

add_library(MoleUtils INTERFACE)

target_sources(MoleUtils INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/include/utils/result.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/include/utils/string_hash.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/include/utils/counter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/include/utils/uuid.hpp
)

target_include_directories(MoleUtils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/utils/include)

add_library(MoleOSAL STATIC)

target_sources(MoleOSAL
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/windowing/glfw/glfw_window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/windowing/glfw/glfw_window.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/windowing/iwindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/inputs/glfw/glfw_inputs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/inputs/glfw/glfw_inputs.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/file_system/ifile_system.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/file_system/windows_file_system.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/file_system/windows_file_system.hpp

        ${CMAKE_CURRENT_SOURCE_DIR}/osal/include/osal/iwindow.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/include/osal/inputs.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/include/osal/input_codes.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/include/osal/ifile_system.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/include/osal/os_context.hpp
)

target_include_directories(MoleOSAL PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/osal/include
        "${CMAKE_SOURCE_DIR}/external/json/include"
)
target_include_directories(MoleOSAL PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(MoleOSAL PUBLIC
        glm::glm
)

target_link_libraries(MoleOSAL PRIVATE
        glfw
        MoleUtils
)

add_library(MoleResources STATIC)

target_sources(MoleResources
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_pool.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_types.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/asset_handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/stbi_image.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/material_pool.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/render_commands.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/asset_handle.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/asset_types.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/asset_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/resources_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/material.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/material_types.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/include/resources/material_pool.hpp
)

target_include_directories(MoleResources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/resources/include)
target_include_directories(MoleResources PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_SOURCE_DIR}/external/stb"
)

target_link_libraries(MoleResources PUBLIC
        MoleOSAL
        MoleUtils
)

add_library(MoleNodeSystem STATIC)

target_sources(MoleNodeSystem
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/render_gatherer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/scene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/scene_factory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/scene_serde.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/managers/drawable_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/managers/transform_manager.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/inode_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/node.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/render_gatherer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/node_types.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/scene.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/scene_factory.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/managers/drawable_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include/node_system/managers/transform_manager.hpp
)

target_include_directories(MoleNodeSystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/node_system/include)
target_include_directories(MoleNodeSystem PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_SOURCE_DIR}/external/json/include"
)

target_link_libraries(MoleNodeSystem PUBLIC MoleResources)

target_link_libraries(MoleNodeSystem PRIVATE
        glm::glm
)

add_library(MoleRendering STATIC)

target_sources(MoleRendering
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/irenderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/metal/texture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_renderer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vk_mem_alloc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_memory_allocator.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_memory_allocator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_swapchain.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_swapchain.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_pipeline.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_pipeline_cache.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_pipeline_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_material_handler.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/vulkan_material_handler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/quad.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/combined_shader_key.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/vulkan/render_batch.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/bindless_texture_registry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/material_shader_layout_reflector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/packed_material_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/material_packer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/material_shader_layout_registry.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/irenderer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/packed_material.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/bindless_texture_registry.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/material_shader_layout_reflector.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/material_shader_layout_registry.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/packed_material_cache.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/material_packer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/metal/texture.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include/rendering/vertex.hpp
)

target_compile_definitions(MoleRendering PRIVATE
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        VULKAN_HPP_NO_EXCEPTIONS
        VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)

if(WIN32)
    target_compile_definitions(MoleRendering PRIVATE
            NOMINMAX
            VK_USE_PLATFORM_WIN32_KHR
    )
endif()

target_include_directories(MoleRendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include)
target_include_directories(MoleRendering PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_SOURCE_DIR}/external/stb"
        "${CMAKE_SOURCE_DIR}/external/Vulkan-Headers/include"
)

target_link_libraries(MoleRendering PUBLIC MoleResources)

target_link_libraries(MoleRendering PRIVATE
        glm::glm
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-msl
        glslang
        glslang-default-resource-limits
        SPIRV
        GPUOpen::VulkanMemoryAllocator
        VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
)

add_library(MoleCore INTERFACE)

target_link_libraries(MoleCore INTERFACE
        MoleNodeSystem
        MoleRendering
        MoleResources
        MoleOSAL
)

if (WIN32)
    find_package(Vulkan REQUIRED)
endif()

target_link_libraries(MoleCore
        INTERFACE
        glm::glm
)

if (Vulkan_FOUND)
    target_link_libraries(MoleCore INTERFACE Vulkan::Vulkan)
endif()

if (APPLE)
    target_link_libraries(MoleCore INTERFACE
            "-framework Metal"
            "-framework Foundation"
            "-framework QuartzCore"
            "-framework Cocoa"
    )
endif()

target_include_directories(MoleCore INTERFACE
        "${CMAKE_SOURCE_DIR}/external/metal-cpp"
        "${CMAKE_SOURCE_DIR}/external/glslang"
        "${CMAKE_SOURCE_DIR}/external/spirv-cross"
        "${CMAKE_SOURCE_DIR}/external"
        "${CMAKE_SOURCE_DIR}/external/stb"
        "${CMAKE_SOURCE_DIR}/external/json/include"
        "${CMAKE_SOURCE_DIR}/external/Vulkan-Headers/include"
        ${CMAKE_CURRENT_SOURCE_DIR}
)