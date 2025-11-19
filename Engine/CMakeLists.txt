add_library(OakenEngine STATIC
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
)

target_include_directories(OakenEngine PUBLIC Source)
target_include_directories(OakenEngine PRIVATE ${SOL2_INCLUDE_DIRS})

target_link_libraries(OakenEngine PUBLIC
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
