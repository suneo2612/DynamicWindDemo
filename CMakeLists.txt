cmake_minimum_required(VERSION 3.25)
project(DynamicWindDemo VERSION 0.1.0 LANGUAGES C CXX)

option(DYNAMIC_WIND_ENABLE_DEBUG_MOTORS "Enable debug-only wind motor slots" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# GLEW 2.2.0 predates the policy minimum required by CMake 4.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
find_package(OpenGL REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(glew-cmake_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ONLY_LIBS ON CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    URL https://codeload.github.com/glfw/glfw/zip/refs/tags/3.4
    URL_HASH SHA256=a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(glew
    URL https://codeload.github.com/Perlmint/glew-cmake/zip/refs/tags/glew-cmake-2.2.0
    URL_HASH SHA256=47a6c6b5c624a07f6178783daebdc273345b359d45e983fcdd844d327bd6e65b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(imgui
    URL https://codeload.github.com/ocornut/imgui/zip/refs/tags/v1.91.9b
    URL_HASH SHA256=fd37507c8476a6d14cc7c4b352401f31bcbd0f0d995d35390811e968c466f46e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(glfw glew imgui)

FetchContent_Declare(ufbx
    URL https://codeload.github.com/ufbx/ufbx/zip/refs/tags/v0.23.1
    URL_HASH SHA256=3e5d8219f5b634fd374949808206eba1a1da49b2b2e3ee31ff0bd228c0abd469
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(ufbx)
if(NOT ufbx_POPULATED)
    FetchContent_Populate(ufbx)
endif()
add_library(ufbx STATIC "${ufbx_SOURCE_DIR}/ufbx.c")
target_include_directories(ufbx SYSTEM PUBLIC "${ufbx_SOURCE_DIR}")
target_compile_definitions(ufbx PRIVATE UFBX_ENABLE_ERROR_STACK)

FetchContent_Declare(stb
    URL https://codeload.github.com/nothings/stb/zip/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
    URL_HASH SHA256=a1e19ce41ddf380fd2291937d9c5616f64a9c224f1f455a8195f87b75b19ba03
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()
add_library(stb_image_impl STATIC solutions/third_party/stb_image_impl.cpp)
target_include_directories(stb_image_impl SYSTEM PUBLIC "${stb_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stb_image_impl PRIVATE /utf-8)
endif()

add_library(demo_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(demo_imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_link_libraries(demo_imgui PUBLIC glfw OpenGL::GL)

set(WIND_SHADER_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/demo/wind/simulation/shader")
set(WIND_DEBUG_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/demo/wind/debug")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    shaders/model.vert shaders/model.frag
    "${WIND_SHADER_DIRECTORY}/clear.comp"
    "${WIND_SHADER_DIRECTORY}/scatter.comp"
    "${WIND_SHADER_DIRECTORY}/finish.comp"
    "${WIND_SHADER_DIRECTORY}/motor.comp"
    "${WIND_DEBUG_DIRECTORY}/shader/wind_debug.vert"
    "${WIND_DEBUG_DIRECTORY}/shader/wind_debug.frag"
    "${WIND_DEBUG_DIRECTORY}/shader/wind_arrow.vert"
    "${WIND_DEBUG_DIRECTORY}/shader/wind_arrow.frag")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/model.vert" MODEL_VERTEX_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/model.frag" MODEL_FRAGMENT_SOURCE)
file(READ "${WIND_SHADER_DIRECTORY}/clear.comp" WIND_CLEAR_SOURCE)
file(READ "${WIND_SHADER_DIRECTORY}/scatter.comp" WIND_SCATTER_SOURCE)
file(READ "${WIND_SHADER_DIRECTORY}/finish.comp" WIND_FINISH_SOURCE)
file(READ "${WIND_SHADER_DIRECTORY}/motor.comp" WIND_MOTOR_SOURCE)
file(READ "${WIND_DEBUG_DIRECTORY}/shader/wind_debug.vert" WIND_DEBUG_VERTEX_SOURCE)
file(READ "${WIND_DEBUG_DIRECTORY}/shader/wind_debug.frag" WIND_DEBUG_FRAGMENT_SOURCE)
file(READ "${WIND_DEBUG_DIRECTORY}/shader/wind_arrow.vert" WIND_ARROW_VERTEX_SOURCE)
file(READ "${WIND_DEBUG_DIRECTORY}/shader/wind_arrow.frag" WIND_ARROW_FRAGMENT_SOURCE)
configure_file(cmake/shader_sources.hpp.in generated/shader_sources.hpp @ONLY)

file(GLOB_RECURSE DYNAMIC_WIND_DEMO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/*.h")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src" PREFIX "src" FILES ${DYNAMIC_WIND_DEMO_SOURCES})

set(DYNAMIC_WIND_DEMO_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/main.cpp")
set(DYNAMIC_WIND_DEMO_APPLICATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/demo/application/application.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/demo/application/imgui_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/demo/application/stage_assets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src/demo/application/stage_controls.cpp")
set(WIND_DEBUG_SOURCES
    "${WIND_DEBUG_DIRECTORY}/wind_debug.cpp"
    "${WIND_DEBUG_DIRECTORY}/wind_debug.h"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_internal.h"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_geometry.cpp"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_geometry.h"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_renderer.cpp"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_renderer.h"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_preview.cpp"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_preview.h"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_preview_color.h"
    "${WIND_DEBUG_DIRECTORY}/wind_debug_controls.cpp")
set(DYNAMIC_WIND_DEMO_SUPPORT_SOURCES ${DYNAMIC_WIND_DEMO_SOURCES})
list(REMOVE_ITEM DYNAMIC_WIND_DEMO_SUPPORT_SOURCES "${DYNAMIC_WIND_DEMO_MAIN_SOURCE}")
list(REMOVE_ITEM DYNAMIC_WIND_DEMO_SUPPORT_SOURCES
    ${DYNAMIC_WIND_DEMO_APPLICATION_SOURCES}
    ${WIND_DEBUG_SOURCES}
    "${WIND_DEBUG_DIRECTORY}/field_pick.cpp"
    "${WIND_DEBUG_DIRECTORY}/field_pick.h")

add_library(dynamic_wind_demo_sources STATIC ${DYNAMIC_WIND_DEMO_SUPPORT_SOURCES})
if(DYNAMIC_WIND_ENABLE_DEBUG_MOTORS)
    target_compile_definitions(dynamic_wind_demo_sources PUBLIC DYNAMIC_WIND_ENABLE_DEBUG_MOTORS)
endif()
target_link_libraries(dynamic_wind_demo_sources PRIVATE ufbx stb_image_impl libglew_static OpenGL::GL)
target_include_directories(dynamic_wind_demo_sources PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(dynamic_wind_demo_sources PRIVATE GLFW_INCLUDE_NONE NOMINMAX)
add_executable(dynamic_wind_demo "${DYNAMIC_WIND_DEMO_MAIN_SOURCE}")
target_sources(dynamic_wind_demo PRIVATE ${DYNAMIC_WIND_DEMO_APPLICATION_SOURCES})
if(DYNAMIC_WIND_ENABLE_DEBUG_MOTORS)
    target_sources(dynamic_wind_demo PRIVATE ${WIND_DEBUG_SOURCES}
                                             "${WIND_DEBUG_DIRECTORY}/field_pick.cpp"
                                             "${WIND_DEBUG_DIRECTORY}/field_pick.h")
endif()
target_include_directories(dynamic_wind_demo PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/solutions/src")
target_link_libraries(dynamic_wind_demo PRIVATE dynamic_wind_demo_sources demo_imgui libglew_static OpenGL::GL)
target_compile_definitions(dynamic_wind_demo PRIVATE GLFW_INCLUDE_NONE NOMINMAX)
target_compile_definitions(dynamic_wind_demo PRIVATE
    DYNAMIC_WIND_STAGE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/solutions/model/stage")
if(MSVC)
    target_compile_options(dynamic_wind_demo PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_options(dynamic_wind_demo_sources PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(dynamic_wind_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(dynamic_wind_demo_sources PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT dynamic_wind_demo)

