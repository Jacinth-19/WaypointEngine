# Waypoint Engine — the `waypoint` player.
# SPDX-License-Identifier: Apache-2.0
add_executable(waypoint main.cpp)
target_link_libraries(waypoint PRIVATE Waypoint::Engine)

set_target_properties(waypoint PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Keep the compiled shaders next to the executable so `./bin/waypoint <project>`
# works from the build folder. waypoint_attach_shaders already copies them when
# rendering is enabled; this covers the case where the runtime is built alone.
if(WAYPOINT_ENABLE_RENDER AND TARGET waypoint_shaders)
    add_dependencies(waypoint waypoint_shaders)
endif()

install(TARGETS waypoint RUNTIME DESTINATION bin)
