set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

add_executable(MoleEditor)

target_sources(MoleEditor
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/editor.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/editor_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/project.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/project_assets.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/file_watcher.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/subprocess.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/states/ieditor_state.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/states/workspace_state.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/process_orchestrator.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/project_asset_pipeline.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/iim_gui_bridge.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/icommand.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/editor/command_history.hpp

        ${CMAKE_CURRENT_SOURCE_DIR}/editor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/project.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/project_assets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/file_watcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/subprocess.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/states/workspace_state.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/process_orchestrator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/project_asset_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/command_history.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/im_gui_bridge/iim_gui_bridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/im_gui_bridge/win32_vulkan_im_gui_bridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/im_gui_bridge/win32_vulkan_im_gui_bridge.hpp
)

target_link_libraries(MoleEditor
        PRIVATE
        MoleCore
        glfw
        reproc++
        imgui
)

target_include_directories(MoleEditor
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_SOURCE_DIR}/external/watcher/include
)

if(WIN32)
    target_compile_definitions(MoleEditor PRIVATE NOMINMAX MOLE_PLATFORM_WINDOWS)
    target_sources(MoleEditor PRIVATE
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    )
endif()

if(APPLE)
    target_compile_definitions(MoleEditor PRIVATE MOLE_PLATFORM_APPLE)
    target_link_libraries(MoleEditor
            PRIVATE
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework MetalKit"
            "-framework CoreGraphics"
            "-framework CoreFoundation"
            "-framework CoreServices"
    )

    target_include_directories(MoleEditor PRIVATE
            "${CMAKE_SOURCE_DIR}/external/metal-cpp"
    )
endif()

# Runtime files the editor loads from exe:// (d_imgui.ini, tools/*.py), re-copied whenever they change
file(GLOB_RECURSE EDITOR_TOOL_SCRIPTS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.py)
set(EDITOR_RUNTIME_FILES d_imgui.ini ${EDITOR_TOOL_SCRIPTS})

set(EDITOR_RUNTIME_OUTPUTS)
foreach(file ${EDITOR_RUNTIME_FILES})
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file}
            COMMENT "Copying ${file}"
    )
    list(APPEND EDITOR_RUNTIME_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/${file})
endforeach()

add_custom_target(MoleEditorRuntimeFiles DEPENDS ${EDITOR_RUNTIME_OUTPUTS})
add_dependencies(MoleEditor MoleEditorRuntimeFiles)

# Embedded Python runtime in exe://python/, set up at configure time
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupPython.cmake)