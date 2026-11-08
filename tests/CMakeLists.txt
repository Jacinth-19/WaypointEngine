# Waypoint Engine — tests.
# SPDX-License-Identifier: Apache-2.0
#
# One executable, one CTest entry. Keeping it to a single binary means
# `ctest` output stays readable and the link step happens once.
waypoint_dependency_doctest()

set(WAYPOINT_TEST_SOURCES
    main.cpp
    test_core.cpp
    test_ecs.cpp
    test_scene.cpp
    test_project.cpp
    test_input.cpp
    test_camera.cpp
    test_content.cpp
)

if(WAYPOINT_ENABLE_SCRIPTING)
    list(APPEND WAYPOINT_TEST_SOURCES test_scripting.cpp)
endif()

if(WAYPOINT_ENABLE_PHYSICS)
    list(APPEND WAYPOINT_TEST_SOURCES test_physics.cpp)
endif()

add_executable(waypoint_tests ${WAYPOINT_TEST_SOURCES})
target_link_libraries(waypoint_tests
    PRIVATE
        Waypoint::Engine
        doctest::doctest
        waypoint_compiler_options
)

# The content test reads templates/ and samples/ from the source tree.
target_compile_definitions(waypoint_tests PRIVATE
    WAYPOINT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME waypoint_tests COMMAND waypoint_tests)
set_tests_properties(waypoint_tests PROPERTIES TIMEOUT 120)
