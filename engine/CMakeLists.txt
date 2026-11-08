# Waypoint Engine — the engine static library.
# SPDX-License-Identifier: Apache-2.0
#
# One library (`Waypoint::Engine`) holds every module. Each optional module is
# only compiled when its CMake option is on, and its headers are guarded by the
# matching WAYPOINT_HAS_* define.

waypoint_dependency_glm()
waypoint_dependency_entt()
waypoint_dependency_json()
waypoint_dependency_spdlog()
# The screenshot writer uses stb_image_write. It has nothing to do with the GPU,
# so it stays outside WAYPOINT_ENABLE_RENDER: the headless suite tests it.
waypoint_dependency_stb()
# glTF assets: the loader is CPU-side, so it belongs to the always-on set rather
# than to the renderer.
waypoint_dependency_tinygltf()

set(WAYPOINT_ENGINE_SOURCES
    core/FileSystem.cpp
    core/ImageWrite.cpp
    assets/GltfLoader.cpp
    assets/StbImage.cpp
    core/Log.cpp
    core/Uuid.cpp
    ecs/World.cpp
    scene/Scene.cpp
    scene/Systems.cpp
    project/Project.cpp
    platform/Input.cpp
)

if(WAYPOINT_ENABLE_SCRIPTING)
    waypoint_dependency_lua()
    waypoint_dependency_sol2()
    list(APPEND WAYPOINT_ENGINE_SOURCES script/LuaScriptEngine.cpp)
endif()

if(WAYPOINT_ENABLE_PHYSICS)
    waypoint_dependency_jolt()
    list(APPEND WAYPOINT_ENGINE_SOURCES physics/JoltPhysicsWorld.cpp)
endif()

if(WAYPOINT_ENABLE_RENDER)
    waypoint_dependency_sdl()
    waypoint_dependency_bgfx()
    list(APPEND WAYPOINT_ENGINE_SOURCES
        platform/SdlWindow.cpp
        render/BgfxRenderer.cpp
        app/Application.cpp
    )
endif()

add_library(waypoint_engine STATIC ${WAYPOINT_ENGINE_SOURCES})
add_library(Waypoint::Engine ALIAS waypoint_engine)

target_include_directories(waypoint_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(waypoint_engine PRIVATE waypoint_compiler_options)
target_link_libraries(waypoint_engine
    PUBLIC
        glm::glm
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        spdlog::spdlog
    PRIVATE
        stb::stb
        tinygltf::tinygltf
)
set_target_properties(waypoint_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Feature defines: `#if WAYPOINT_HAS_PHYSICS` etc. Defaults to 0 so code compiles
# even when the module is switched off.
waypoint_feature_define(waypoint_engine RENDER ${WAYPOINT_ENABLE_RENDER})
waypoint_feature_define(waypoint_engine PHYSICS ${WAYPOINT_ENABLE_PHYSICS})
waypoint_feature_define(waypoint_engine SCRIPTING ${WAYPOINT_ENABLE_SCRIPTING})
waypoint_feature_define(waypoint_engine APP ${WAYPOINT_ENABLE_RENDER})

if(WAYPOINT_ENABLE_SCRIPTING)
    target_link_libraries(waypoint_engine PRIVATE lua::lua sol2::sol2)
endif()

if(WAYPOINT_ENABLE_PHYSICS)
    target_link_libraries(waypoint_engine PRIVATE Jolt)
endif()

if(WAYPOINT_ENABLE_RENDER)
    target_link_libraries(waypoint_engine PUBLIC bgfx SDL3::SDL3)
    # Which backend the built-in default uses. Override at runtime with the
    # WAYPOINT_RENDERER environment variable.
    target_compile_definitions(waypoint_engine PRIVATE
        WAYPOINT_DEFAULT_RENDERER="${WAYPOINT_RENDERER}")
    waypoint_attach_shaders(waypoint_engine)
endif()
