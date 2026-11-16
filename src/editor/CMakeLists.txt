set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
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

        ${CMAKE_CURRENT_SOURCE_DIR}/editor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/project.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/project_assets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/file_watcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/subprocess.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/states/workspace_state.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/process_orchestrator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/project_asset_pipeline.cpp
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
    target_compile_definitions(MoleEditor PRIVATE NOMINMAX)
endif()

if(APPLE)
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

include(${CMAKE_SOURCE_DIR}/cmake/SetupPython.cmake)
