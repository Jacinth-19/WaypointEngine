# Waypoint Engine — shader compilation.
# SPDX-License-Identifier: Apache-2.0
#
# bgfx eats pre-compiled shader binaries, not GLSL source. This module drives
# `shaderc` (built from the bgfx checkout) to turn shaders/*.sc into
# `<build>/shaders/<profile>/<name>.bin`, matching the folder layout the runtime
# looks in.
#
# Gotchas encoded here:
#   * bgfxToolUtils.cmake is only pulled in by bgfx.cmake's own examples, so we
#     include it ourselves.
#   * `INCLUDE_DIRS ${bgfx}/src` is required for `bgfx_shader.sh`; the variable
#     BGFX_SHADER_INCLUDE_PATH only exists in an installed bgfx config.
#   * Output lands in <out>/<profile>/ with 300_es->essl, 430->glsl,
#     s_5_0->dxbc, s_6_0->dxil.

include_guard(GLOBAL)

set(WAYPOINT_SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../shaders" CACHE PATH
    "Folder holding *.sc shader sources")
set(WAYPOINT_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders" CACHE PATH
    "Folder shader binaries are written to")

# Compiles the engine's shaders and makes `target` depend on them.
# Safe to call when rendering is disabled: it simply does nothing.
function(waypoint_attach_shaders target)
    if(NOT WAYPOINT_ENABLE_RENDER)
        return()
    endif()

    if(NOT TARGET bgfx::shaderc)
        message(WARNING
            "Waypoint: shaderc is not available (WAYPOINT_BUILD_BGFX_TOOLS=${WAYPOINT_BUILD_BGFX_TOOLS}). "
            "Shaders will not be compiled; the runtime will show its 'shaders missing' fallback.")
        return()
    endif()

    if(NOT COMMAND bgfx_compile_shaders)
        include("${WAYPOINT_DEP_bgfx_cmake_SOURCE_DIR}/cmake/bgfxToolUtils.cmake")
    endif()
    if(NOT COMMAND bgfx_compile_shaders)
        message(WARNING "Waypoint: bgfxToolUtils.cmake did not define bgfx_compile_shaders")
        return()
    endif()

    set(varying_def "${WAYPOINT_SHADER_SOURCE_DIR}/varying.def.sc")
    # bgfx's own shader headers live in the bgfx checkout, not in a package.
    set(include_dirs "${WAYPOINT_DEP_bgfx_SOURCE_DIR}/src" "${WAYPOINT_SHADER_SOURCE_DIR}")

    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS "${WAYPOINT_SHADER_SOURCE_DIR}/vs_basic.sc"
        VARYING_DEF "${varying_def}"
        OUTPUT_DIR "${WAYPOINT_SHADER_OUTPUT_DIR}"
        OUT_FILES_VAR WAYPOINT_VERTEX_SHADER_FILES
        INCLUDE_DIRS ${include_dirs}
    )
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS "${WAYPOINT_SHADER_SOURCE_DIR}/fs_basic.sc"
        VARYING_DEF "${varying_def}"
        OUTPUT_DIR "${WAYPOINT_SHADER_OUTPUT_DIR}"
        OUT_FILES_VAR WAYPOINT_FRAGMENT_SHADER_FILES
        INCLUDE_DIRS ${include_dirs}
    )

    add_custom_target(waypoint_shaders DEPENDS
                      ${WAYPOINT_VERTEX_SHADER_FILES} ${WAYPOINT_FRAGMENT_SHADER_FILES})
    add_dependencies(${target} waypoint_shaders)

    target_compile_definitions(${target} PRIVATE
        WAYPOINT_SHADER_DIR="${WAYPOINT_SHADER_OUTPUT_DIR}")

    # Keep a copy next to the executables so `waypoint <project>` works from the
    # build folder without setting any environment variables.
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${WAYPOINT_SHADER_OUTPUT_DIR}" "$<TARGET_FILE_DIR:${target}>/shaders"
        COMMENT "Copying compiled shaders next to ${target}"
        VERBATIM
    )
endfunction()
