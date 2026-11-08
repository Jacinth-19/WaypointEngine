# Waypoint Engine — top-level build.
# SPDX-License-Identifier: Apache-2.0
#
# Quick start (Windows / Linux / macOS):
#   cmake --preset default
#   cmake --build --preset default
# or simply:
#   python tools/wp.py build
#
# Containers and CI without a GPU use:
#   WAYPOINT_HEADLESS_BUILD=1 python tools/wp.py test
cmake_minimum_required(VERSION 3.24)

project(
    WaypointEngine
    VERSION 0.1.0
    DESCRIPTION "A beginner-friendly C++20 engine for urban open-world games"
    HOMEPAGE_URL "https://github.com/Jacinth-19/WaypointEngine"
    LANGUAGES C CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options. Every subsystem can be switched off, which is what makes headless
# test builds possible on machines with no GPU or window system.
# ---------------------------------------------------------------------------
option(WAYPOINT_BUILD_RUNTIME "Build the 'waypoint' player executable" ON)
option(WAYPOINT_BUILD_EDITOR "Build the Qt 6 editor" OFF)
option(WAYPOINT_BUILD_TESTS "Build the doctest test suite" ON)
option(WAYPOINT_ENABLE_RENDER "Compile bgfx + SDL3 rendering" ON)
option(WAYPOINT_ENABLE_PHYSICS "Compile Jolt Physics" ON)
option(WAYPOINT_ENABLE_SCRIPTING "Compile Lua + sol2 scripting" ON)
option(WAYPOINT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(WAYPOINT_BUILD_BGFX_TOOLS "Build shaderc so shaders can be compiled" ON)
option(WAYPOINT_HEADLESS_BUILD "Configure without windowing, GPU or Qt (containers, CI)" OFF)

if(WAYPOINT_HEADLESS_BUILD)
    message(STATUS "Waypoint: headless build - rendering, runtime and editor are disabled")
    set(WAYPOINT_ENABLE_RENDER OFF CACHE BOOL "" FORCE)
    set(WAYPOINT_BUILD_RUNTIME OFF CACHE BOOL "" FORCE)
    set(WAYPOINT_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(WAYPOINT_BUILD_BGFX_TOOLS OFF CACHE BOOL "" FORCE)
endif()

set(WAYPOINT_RENDERER "auto" CACHE STRING
    "Preferred bgfx backend: auto, vulkan, opengl, opengles, d3d11, d3d12, metal")
set_property(CACHE WAYPOINT_RENDERER PROPERTY STRINGS
             auto vulkan opengl opengles d3d11 d3d12 metal)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

include(WaypointCompiler)
include(WaypointDependencies)
include(WaypointShaders)

waypoint_set_output_directories()

# ---------------------------------------------------------------------------
# Targets.
# ---------------------------------------------------------------------------
add_subdirectory(engine)

if(WAYPOINT_BUILD_RUNTIME)
    add_subdirectory(runtime)
endif()

if(WAYPOINT_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(WAYPOINT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Friendly summary. Seeing the exact configuration beats guessing why a feature
# is missing.
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Waypoint Engine ${PROJECT_VERSION}")
message(STATUS "  build type      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler        : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  render          : ${WAYPOINT_ENABLE_RENDER} (backend: ${WAYPOINT_RENDERER})")
message(STATUS "  physics         : ${WAYPOINT_ENABLE_PHYSICS}")
message(STATUS "  scripting       : ${WAYPOINT_ENABLE_SCRIPTING}")
message(STATUS "  runtime         : ${WAYPOINT_BUILD_RUNTIME}")
message(STATUS "  editor          : ${WAYPOINT_BUILD_EDITOR}")
message(STATUS "  tests           : ${WAYPOINT_BUILD_TESTS}")
message(STATUS "  headless        : ${WAYPOINT_HEADLESS_BUILD}")
message(STATUS "")
