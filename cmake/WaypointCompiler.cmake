# Waypoint Engine — compile settings shared by every target.
# SPDX-License-Identifier: Apache-2.0
#
# Everything here is attached to one INTERFACE target, so first-party code gets
# strict warnings while third-party code (which we never edit) stays quiet.

add_library(waypoint_compiler_options INTERFACE)
add_library(Waypoint::CompilerOptions ALIAS waypoint_compiler_options)

target_compile_features(waypoint_compiler_options INTERFACE cxx_std_20)

# Keep binaries and object files in predictable places so `wp.py export` and
# the editor can find them without guessing.
function(waypoint_set_output_directories)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" PARENT_SCOPE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" PARENT_SCOPE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" PARENT_SCOPE)
endfunction()

if(MSVC)
    target_compile_options(
        waypoint_compiler_options
        INTERFACE
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /utf-8
            /EHsc
            /MP
    )
    target_compile_definitions(waypoint_compiler_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(WAYPOINT_WARNINGS_AS_ERRORS)
        target_compile_options(waypoint_compiler_options INTERFACE /WX)
    endif()
else()
    target_compile_options(
        waypoint_compiler_options
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wdouble-promotion
    )
    if(WAYPOINT_WARNINGS_AS_ERRORS)
        target_compile_options(waypoint_compiler_options INTERFACE -Werror)
    endif()
endif()

# Debug builds turn on engine assertions; release builds compile them out.
target_compile_definitions(
    waypoint_compiler_options
    INTERFACE $<$<CONFIG:Debug>:WAYPOINT_DEBUG=1> $<$<CONFIG:Debug>:WAYPOINT_ENABLE_ASSERTS=1>
)

# Feature switches become preprocessor defines so code can ask
# `#if WAYPOINT_HAS_PHYSICS` instead of relying on fragile include paths.
function(waypoint_feature_define target feature enabled)
    if(${enabled})
        target_compile_definitions(${target} PUBLIC WAYPOINT_HAS_${feature}=1)
    else()
        target_compile_definitions(${target} PUBLIC WAYPOINT_HAS_${feature}=0)
    endif()
endfunction()
