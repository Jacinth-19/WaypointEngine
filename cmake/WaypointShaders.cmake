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

    # bgfx's own shader headers (bgfx_shader.sh, bgfx_compute.sh) live in the
    # bgfx checkout, not in a package, so shaderc needs an explicit -i for them;
    # without it every shader fails with "Could not open include file".
    waypoint_dependency_dir(bgfx_dir bgfx)
    if(NOT bgfx_dir)
        message(FATAL_ERROR
            "Waypoint: the bgfx checkout is unknown, so bgfx_shader.sh cannot be found. "
            "Call waypoint_dependency_bgfx() before waypoint_attach_shaders().")
    endif()
    set(include_dirs "${bgfx_dir}/src" "${WAYPOINT_SHADER_SOURCE_DIR}")

    # Which backends to bake bytecode for. bgfx.cmake's defaults include dxil on
    # Windows, but that profile goes GLSL -> SPIR-V -> spirv-cross -> DXC and DXC
    # rejects the vertex shader with a spurious
    #   "hlsl.hlsl:624:101: error: expression result unused [-Werror,-Wunused-value]"
    # on the varying write. dxbc (D3D11, the default Windows backend) covers
    # Windows; dxil is only needed by the D3D12 backend, which is not validated
    # yet. See docs/STATUS.md. Turn it back on with -DWAYPOINT_COMPILE_DXIL=ON
    # once that path is checked on real hardware.
    # These are shaderc profile names, not folder names: bgfx maps 430 -> glsl,
    # 300_es -> essl, s_5_0 -> dxbc, s_6_0 -> dxil.
    option(WAYPOINT_COMPILE_DXIL "Compile dxil (D3D12) shaders" OFF)
    if(WIN32)
        set(profiles spirv 430 300_es s_5_0)
        if(WAYPOINT_COMPILE_DXIL)
            list(APPEND profiles s_6_0)
        endif()
    elseif(APPLE)
        set(profiles spirv 430 300_es metal)
    else()
        set(profiles spirv 430 300_es)
    endif()

    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS "${WAYPOINT_SHADER_SOURCE_DIR}/vs_basic.sc"
        VARYING_DEF "${varying_def}"
        OUTPUT_DIR "${WAYPOINT_SHADER_OUTPUT_DIR}"
        OUT_FILES_VAR WAYPOINT_VERTEX_SHADER_FILES
        PROFILES ${profiles}
        INCLUDE_DIRS ${include_dirs}
    )
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS "${WAYPOINT_SHADER_SOURCE_DIR}/fs_basic.sc"
        VARYING_DEF "${varying_def}"
        OUTPUT_DIR "${WAYPOINT_SHADER_OUTPUT_DIR}"
        OUT_FILES_VAR WAYPOINT_FRAGMENT_SHADER_FILES
        PROFILES ${profiles}
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
