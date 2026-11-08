# Waypoint Engine — third-party dependencies.
# SPDX-License-Identifier: Apache-2.0
#
# One rule: the same pins are used everywhere.
#   * `external/dependencies.lock.json` is the source of truth.
#   * `python tools/wp.py deps fetch` clones those pins into `external/<name>`.
#   * If a checkout is missing, CMake fetches the same pin with FetchContent into
#     `.cache/waypoint-deps` (CI caches that folder).
#
# We never use git submodules; see docs/DEPENDENCIES.md for why.

include_guard(GLOBAL)

include(FetchContent)

set(WAYPOINT_EXTERNAL_DIR "${CMAKE_CURRENT_LIST_DIR}/../external" CACHE PATH
    "Where `wp.py deps fetch` puts dependency checkouts")
set(WAYPOINT_LOCKFILE "${CMAKE_CURRENT_LIST_DIR}/../external/dependencies.lock.json" CACHE FILEPATH
    "Dependency lockfile; all pins are read from here")
set(WAYPOINT_DEPENDENCY_CACHE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.cache/waypoint-deps" CACHE PATH
    "FetchContent download area (cache this folder in CI)")
set(FETCHCONTENT_BASE_DIR "${WAYPOINT_DEPENDENCY_CACHE_DIR}" CACHE PATH
    "FetchContent download area (cache this folder in CI)")
set(FETCHCONTENT_QUIET ON)

# FetchContent_Populate(<name>) is the only way to download a dependency without
# also running its own CMakeLists. CMake 3.30+ deprecates it; keep the old
# behaviour deliberately, because we do not trust every upstream CMake project.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

file(READ "${WAYPOINT_LOCKFILE}" WAYPOINT_LOCK_JSON)

# Reads one field of one dependency from the lockfile.
function(waypoint_lock_get out_var name field)
    string(JSON value ERROR_VARIABLE lock_error GET "${WAYPOINT_LOCK_JSON}"
           dependencies "${name}" "${field}")
    if(lock_error)
        set(${out_var} "" PARENT_SCOPE)
    else()
        set(${out_var} "${value}" PARENT_SCOPE)
    endif()
endfunction()

# Silences compiler warnings coming from third-party sources we do not own.
function(waypoint_silence_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /w)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endfunction()

# Returns the source directory chosen for `name` (empty if it was never
# resolved). WAYPOINT_DEP_<name>_SOURCE_DIR only lives in the scope of the
# waypoint_dependency_<name>() function that set it, so anything that needs the
# path later - shader compilation needs bgfx/src for bgfx_shader.sh - asks here.
function(waypoint_dependency_dir out_var name)
    get_property(source GLOBAL PROPERTY "WAYPOINT_SOURCE_${name}")
    set(${out_var} "${source}" PARENT_SCOPE)
endfunction()

# Applies any local patch in cmake/patches/<name>-*.patch to a checkout.
#
# A pin is supposed to be pristine upstream code, so patches stay rare, live in
# tracked files, and are applied here and by `wp.py deps fetch` so that every
# dependency path ends up byte-identical. Re-running is a no-op.
function(waypoint_apply_dependency_patches name source)
    file(GLOB patches "${CMAKE_CURRENT_LIST_DIR}/patches/${name}-*.patch")
    foreach(patch IN LISTS patches)
        get_filename_component(patch_name "${patch}" NAME)
        execute_process(
            COMMAND git -C "${source}" apply --check --reverse "${patch}"
            RESULT_VARIABLE already_applied
            OUTPUT_QUIET
            ERROR_QUIET)
        if(already_applied EQUAL 0)
            continue()
        endif()
        execute_process(
            COMMAND git -C "${source}" apply "${patch}"
            RESULT_VARIABLE apply_result
            OUTPUT_VARIABLE apply_output
            ERROR_VARIABLE apply_output)
        if(NOT apply_result EQUAL 0)
            message(FATAL_ERROR
                "Waypoint: could not apply ${patch_name} to ${name}: ${apply_output}")
        endif()
        message(STATUS "Waypoint: applied ${patch_name} to ${name}")
    endforeach()
endfunction()

# Guarantees the source for `name` is available and stores the path in
# WAYPOINT_DEP_<name>_SOURCE_DIR.
#
# Order of preference:
#   1. -DWAYPOINT_DEP_<NAME>_DIR=... (escape hatch)
#   2. external/<name>/.git       (wp.py deps fetch, or a hand-made checkout)
#   3. FetchContent at the pinned commit
function(waypoint_dependency_source name)
    string(TOUPPER "${name}" NAME_UPPER)
    set(cache_var "WAYPOINT_SOURCE_${name}")

    get_property(known GLOBAL PROPERTY "${cache_var}" SET)
    if(known)
        get_property(source GLOBAL PROPERTY "${cache_var}")
        set(WAYPOINT_DEP_${name}_SOURCE_DIR "${source}" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED WAYPOINT_DEP_${NAME_UPPER}_DIR AND EXISTS "${WAYPOINT_DEP_${NAME_UPPER}_DIR}")
        message(STATUS "Waypoint: ${name} from -DWAYPOINT_DEP_${NAME_UPPER}_DIR")
        set(source "${WAYPOINT_DEP_${NAME_UPPER}_DIR}")
    elseif(EXISTS "${WAYPOINT_EXTERNAL_DIR}/${name}/.git")
        message(STATUS "Waypoint: ${name} from external/${name}")
        set(source "${WAYPOINT_EXTERNAL_DIR}/${name}")
    else()
        waypoint_lock_get(repository "${name}" "repository")
        waypoint_lock_get(commit "${name}" "commit")
        if(NOT repository OR NOT commit)
            message(FATAL_ERROR "Waypoint: '${name}' is missing from ${WAYPOINT_LOCKFILE}")
        endif()
        message(STATUS "Waypoint: fetching ${name} at ${commit} (this happens once)")
        FetchContent_Declare(
            ${name}
            GIT_REPOSITORY "${repository}"
            GIT_TAG "${commit}"
            GIT_PROGRESS ON
            GIT_SHALLOW FALSE
        )
        FetchContent_Populate(${name})
        set(source "${${name}_SOURCE_DIR}")
    endif()

    waypoint_apply_dependency_patches(${name} "${source}")
    set_property(GLOBAL PROPERTY "${cache_var}" "${source}")
    set(WAYPOINT_DEP_${name}_SOURCE_DIR "${source}" PARENT_SCOPE)
endfunction()

# Makes a header-only dependency available as an interface target.
function(waypoint_header_only_dependency name target_name include_subdir)
    if(TARGET ${target_name})
        return()
    endif()
    waypoint_dependency_source(${name})
    set(source "${WAYPOINT_DEP_${name}_SOURCE_DIR}")
    add_library(${target_name} INTERFACE IMPORTED GLOBAL)
    if(include_subdir STREQUAL "")
        set(include_path "${source}")
    else()
        set(include_path "${source}/${include_subdir}")
    endif()
    set_target_properties(${target_name} PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${include_path}")
endfunction()

# ---------------------------------------------------------------------------
# One function per dependency. Each creates the conventional target name so
# that `target_link_libraries(... glm::glm)` works like any other project.
# ---------------------------------------------------------------------------

# glm — math library (header only).
function(waypoint_dependency_glm)
    waypoint_header_only_dependency(glm glm::glm ".")
endfunction()

# EnTT — entity component system (header only).
function(waypoint_dependency_entt)
    waypoint_header_only_dependency(entt EnTT::EnTT "src")
endfunction()

# nlohmann/json — JSON reader/writer (header only).
function(waypoint_dependency_json)
    waypoint_header_only_dependency(json nlohmann_json::nlohmann_json "include")
endfunction()

# stb — single-header image loading (header only).
function(waypoint_dependency_stb)
    waypoint_header_only_dependency(stb stb::stb ".")
endfunction()

# tinygltf — glTF model loader (header only; needs stb_image + json).
function(waypoint_dependency_tinygltf)
    waypoint_header_only_dependency(tinygltf tinygltf::tinygltf ".")
    target_compile_definitions(tinygltf::tinygltf INTERFACE TINYGLTF_NO_INCLUDE_STB_IMAGE=0)
endfunction()

# doctest — unit testing (header only).
function(waypoint_dependency_doctest)
    waypoint_header_only_dependency(doctest doctest::doctest ".")
endfunction()

# sol2 — Lua bindings (header only).
function(waypoint_dependency_sol2)
    waypoint_header_only_dependency(sol2 sol2::sol2 "include")
endfunction()

# spdlog — logging. Compiled (not header-only) to keep our build times low.
function(waypoint_dependency_spdlog)
    if(TARGET spdlog::spdlog)
        return()
    endif()
    waypoint_dependency_source(spdlog)
    set(source "${WAYPOINT_DEP_spdlog_SOURCE_DIR}")

    add_library(waypoint_spdlog STATIC
        "${source}/src/spdlog.cpp"
        "${source}/src/stdout_sinks.cpp"
        "${source}/src/color_sinks.cpp"
        "${source}/src/file_sinks.cpp"
        "${source}/src/async.cpp"
        "${source}/src/cfg.cpp"
        "${source}/src/bundled_fmtlib_format.cpp"
    )
    add_library(spdlog::spdlog ALIAS waypoint_spdlog)
    target_include_directories(waypoint_spdlog PUBLIC "${source}/include")
    target_compile_definitions(waypoint_spdlog PUBLIC SPDLOG_COMPILED_LIB)
    target_compile_features(waypoint_spdlog PUBLIC cxx_std_17)
    waypoint_silence_warnings(waypoint_spdlog)
endfunction()

# Lua 5.4 — scripting language runtime. Sources live at the repo root; lua.c and
# luac.c are standalone interpreters and must be left out of the library.
function(waypoint_dependency_lua)
    if(TARGET lua::lua)
        return()
    endif()
    waypoint_dependency_source(lua)
    set(source "${WAYPOINT_DEP_lua_SOURCE_DIR}")

    file(GLOB lua_sources CONFIGURE_DEPENDS "${source}/*.c")
    list(FILTER lua_sources EXCLUDE REGEX "/(lua|luac|ltests)\\.c$")
    if(NOT lua_sources)
        message(FATAL_ERROR "Waypoint: no Lua sources found in ${source}")
    endif()

    add_library(waypoint_lua STATIC ${lua_sources})
    add_library(lua::lua ALIAS waypoint_lua)
    target_include_directories(waypoint_lua PUBLIC "${source}")
    set_target_properties(waypoint_lua PROPERTIES LINKER_LANGUAGE C)
    if(UNIX AND NOT APPLE)
        target_link_libraries(waypoint_lua PUBLIC m ${CMAKE_DL_LIBS})
    endif()
    waypoint_silence_warnings(waypoint_lua)
endfunction()

# Jolt Physics — collision detection and rigid bodies.
function(waypoint_dependency_jolt)
    if(TARGET Jolt)
        return()
    endif()
    waypoint_dependency_source(jolt)
    set(source "${WAYPOINT_DEP_jolt_SOURCE_DIR}")

    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${source}/Build" "${CMAKE_BINARY_DIR}/_waypoint_deps/jolt" EXCLUDE_FROM_ALL)
    waypoint_silence_warnings(Jolt)
endfunction()

# SDL3 — window, keyboard and mouse. Static so exported games are self-contained.
function(waypoint_dependency_sdl)
    if(TARGET SDL3::SDL3)
        return()
    endif()
    waypoint_dependency_source(sdl)
    set(source "${WAYPOINT_DEP_sdl_SOURCE_DIR}")

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)
    set(SDL_WERROR OFF CACHE BOOL "" FORCE)
    add_subdirectory("${source}" "${CMAKE_BINARY_DIR}/_waypoint_deps/sdl" EXCLUDE_FROM_ALL)
    waypoint_silence_warnings(SDL3-static)
endfunction()

# bgfx + bx + bimg — cross-platform GPU rendering.
#
# Gotcha: bgfx.cmake must be told where bx, bimg and bgfx live. It also calls
# find_package(X11 REQUIRED) on Linux even for a tools-only build, so a headless
# configure must leave WAYPOINT_ENABLE_RENDER off.
function(waypoint_dependency_bgfx)
    if(TARGET bgfx)
        return()
    endif()

    waypoint_dependency_source(bgfx)
    waypoint_dependency_source(bx)
    waypoint_dependency_source(bimg)
    waypoint_dependency_source(bgfx_cmake)

    set(BGFX_DIR "${WAYPOINT_DEP_bgfx_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(BX_DIR "${WAYPOINT_DEP_bx_SOURCE_DIR}" CACHE PATH "" FORCE)
    set(BIMG_DIR "${WAYPOINT_DEP_bimg_SOURCE_DIR}" CACHE PATH "" FORCE)

    set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_DEBUG ${WAYPOINT_DEBUG_RENDERER} CACHE BOOL "")
    set(BGFX_BUILD_TOOLS ${WAYPOINT_BUILD_BGFX_TOOLS} CACHE BOOL "" FORCE)
    set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BIMG_INSTALL OFF CACHE BOOL "" FORCE)

    add_subdirectory("${WAYPOINT_DEP_bgfx_cmake_SOURCE_DIR}"
                     "${CMAKE_BINARY_DIR}/_waypoint_deps/bgfx" EXCLUDE_FROM_ALL)

    # bgfx links bx PRIVATE, so bx's headers are not visible through bgfx. The
    # engine itself uses glm for maths, but expose bx anyway: any caller doing
    # bx::mtxLookAt-style work would otherwise fail to compile.
    if(TARGET bgfx AND EXISTS "${WAYPOINT_DEP_bx_SOURCE_DIR}/include")
        target_include_directories(bgfx INTERFACE "${WAYPOINT_DEP_bx_SOURCE_DIR}/include")
    endif()
endfunction()
