add_library(OakenEngine SHARED
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
    Source/Resources/ResourceManager.h
    Source/Resources/ResourceManager.cpp
    Source/Resources/Texture.h
    Source/Resources/Texture.cpp
    Source/Resources/Mesh.h
    Source/Resources/Mesh.cpp
    Source/Resources/Skeleton.h
    Source/Resources/Skeleton.cpp
    Source/Resources/Animation.h
    Source/Resources/Animation.cpp
    Source/Resources/Shader.h
    Source/Resources/Shader.cpp
    Source/Scene/Scene.h
    Source/Scene/Scene.cpp
    Source/Scene/SceneManager.h
    Source/Scene/SceneManager.cpp
    Source/Scene/SceneSerializer.h
    Source/Scene/SceneSerializer.cpp
    Source/Systems/AbilitySystem.h
    Source/Systems/AbilitySystem.cpp
    Source/Systems/RenderSystem.h
    Source/Systems/RenderSystem.cpp
    Source/Systems/TransformSystem.h
    Source/Systems/TransformSystem.cpp
    Source/Systems/PhysicsSystem.h
    Source/Systems/PhysicsSystem.cpp
    Source/Systems/ScriptSystem.h
    Source/Systems/ScriptSystem.cpp
    Source/Systems/EditorSystem.h
    Source/Systems/EditorSystem.cpp
    Source/Systems/AnimationSystem.h
    Source/Systems/AnimationSystem.cpp
    Source/Systems/CameraSystem.h
    Source/Systems/CameraSystem.cpp
    Source/Systems/CharacterSystem.h
    Source/Systems/CharacterSystem.cpp
)

# Auto-export all symbols for DLL creation on Windows
set_target_properties(OakenEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_compile_definitions(OakenEngine PRIVATE OAKEN_EXPORT)

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
    ozz_animation
)
