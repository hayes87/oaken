#pragma once

#include "Core/Context.h"
#include "Core/EventBus.h"
#include "Platform/Window.h"
#include "Platform/Input.h"
#include "Platform/RenderDevice.h"
#include "Resources/ResourceManager.h"
#include "Scene/SceneManager.h"
#include <flecs.h>
#include <memory>

#include "Systems/AbilitySystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/EditorSystem.h"
#include "Systems/TransformSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/CharacterSystem.h"

// ImGui
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#ifdef _WIN32
    #define GAME_EXPORT extern "C" __declspec(dllexport)
#else
    #define GAME_EXPORT extern "C"
#endif

class Engine {
public:
    Engine();
    ~Engine();

    bool Init();
    void Run();
    bool Step(); // Returns false if engine should close
    void Shutdown();

    void SetTimeLimit(double seconds) { m_TimeLimit = seconds; }

    Core::GameContext& GetContext() { return m_Context; }
    Resources::ResourceManager& GetResourceManager() { return *m_ResourceManager; }

private:
    void Update(double dt);
    void Render(double alpha);

    std::unique_ptr<Platform::Window> m_Window;
    std::unique_ptr<Platform::Input> m_Input;
    std::unique_ptr<Platform::RenderDevice> m_RenderDevice;
    std::unique_ptr<Resources::ResourceManager> m_ResourceManager;
    std::unique_ptr<Core::SceneManager> m_SceneManager;
    std::unique_ptr<Core::EventBus> m_EventBus;
    
    Core::GameContext m_Context;
    
    // Time keeping
    double m_Accumulator = 0.0;
    double m_CurrentTime = 0.0;
    double m_TotalTime = 0.0;

    // Systems
    std::unique_ptr<Systems::AbilitySystem> m_AbilitySystem;
    std::unique_ptr<Systems::RenderSystem> m_RenderSystem;
    std::unique_ptr<Systems::PhysicsSystem> m_PhysicsSystem;
    std::unique_ptr<Systems::ScriptSystem> m_ScriptSystem;
    std::unique_ptr<Systems::EditorSystem> m_EditorSystem;
    std::unique_ptr<Systems::TransformSystem> m_TransformSystem;
    std::unique_ptr<Systems::AnimationSystem> m_AnimationSystem;
    std::unique_ptr<Systems::CameraSystem> m_CameraSystem;
    std::unique_ptr<Systems::CharacterSystem> m_CharacterSystem;

    bool m_IsRunning = false;
    bool m_EditorMode = true;
    bool m_DebugPhysics = true;  // Draw physics colliders
    double m_TimeLimit = 0.0;

    // Debug UI state
    bool m_ShowDebugMenu = false;
    bool m_ShowColliders = true;
    bool m_ShowSkeleton = true;
    bool m_ShowFPS = true;
    
    // FPS tracking
    float m_FPSAccumulator = 0.0f;
    int m_FrameCount = 0;
    float m_CurrentFPS = 0.0f;

    void InitImGui();
    void ShutdownImGui();
    void RenderDebugMenu();
    void HandleDebugInput();
    void UpdateFPSCounter(float deltaTime);
};
