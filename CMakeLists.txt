cmake_minimum_required(VERSION 3.25)

project(OakenEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vcpkg integration is assumed to be handled by the toolchain file

find_package(SDL3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_path(SOL2_INCLUDE_DIRS "sol/sol.hpp")
find_package(spdlog CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(OakenEngine
    Source/Engine.h
    Source/Engine.cpp
    Source/Core/HashedString.h
    Source/Core/EventBus.h
    Source/Core/Context.h
    Source/Core/Log.h
    Source/Core/Log.cpp
    Source/Core/TimeStep.h
    Source/Core/Profiler.h
    Source/Components/Components.h
    Source/Components/Reflection.h
    Source/Components/Reflection.cpp
    Source/Platform/Window.h
    Source/Platform/Window.cpp
    Source/Platform/Input.h
    Source/Platform/Input.cpp
    Source/Platform/RenderDevice.h
    Source/Platform/RenderDevice.cpp
    Source/Systems/AbilitySystem.h
    Source/Systems/AbilitySystem.cpp
    Source/Systems/RenderSystem.h
    Source/Systems/RenderSystem.cpp
    Source/Systems/PhysicsSystem.h
    Source/Systems/PhysicsSystem.cpp
    Source/Systems/ScriptSystem.h
    Source/Systems/ScriptSystem.cpp
    Source/Systems/EditorSystem.h
    Source/Systems/EditorSystem.cpp
    Source/main.cpp
)

target_include_directories(OakenEngine PRIVATE ${SOL2_INCLUDE_DIRS})

target_link_libraries(OakenEngine PRIVATE
    SDL3::SDL3
    flecs::flecs
    glm::glm
    Jolt::Jolt
    ${LUA_LIBRARIES}
    spdlog::spdlog
    Tracy::TracyClient
    fastgltf::fastgltf
    imgui::imgui
)

target_include_directories(OakenEngine PRIVATE ${SOL2_INCLUDE_DIRS})

# Copy Shaders to Build Directory
add_custom_command(TARGET OakenEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Source/Shaders
    $<TARGET_FILE_DIR:OakenEngine>/Source/Shaders
    COMMENT "Copying Shaders to Output Directory"
)
