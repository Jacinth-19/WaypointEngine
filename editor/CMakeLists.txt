# Waypoint Engine — Qt 6 editor.
# SPDX-License-Identifier: Apache-2.0
#
# Qt is only needed here. A missing Qt install must not break the engine build,
# which is why WAYPOINT_BUILD_EDITOR defaults to OFF.
find_package(Qt6 6.2 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    message(WARNING
        "Waypoint: Qt 6 Widgets was not found, so the editor cannot be built.\n"
        "  Debian/Ubuntu: sudo apt install qt6-base-dev\n"
        "  macOS:         brew install qt\n"
        "  Windows:       install Qt with the online installer, then set CMAKE_PREFIX_PATH\n"
        "  Continuing without the editor (WAYPOINT_BUILD_EDITOR=ON but Qt is missing).")
    return()
endif()

add_executable(waypoint_editor
    main.cpp
    EditorWindow.cpp
    EditorWindow.hpp
)

target_link_libraries(waypoint_editor
    PRIVATE
        Waypoint::Engine
        Qt6::Widgets
        waypoint_compiler_options
)

# Qt's meta-object compiler turns Q_OBJECT classes into real C++.
set_target_properties(waypoint_editor PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

install(TARGETS waypoint_editor RUNTIME DESTINATION bin)
