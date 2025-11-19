#pragma once

#include "Core/Context.h"
#include "Core/EventBus.h"
#include "Platform/Window.h"
#include "Platform/Input.h"
#include "Platform/RenderDevice.h"
#include <flecs.h>
#include <memory>

#include "Systems/AbilitySystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/EditorSystem.h"

class Engine {
public:
    Engine();
    ~Engine();

    bool Init();
    void Run();
    void Shutdown();

    void SetTimeLimit(double seconds) { m_TimeLimit = seconds; }

private:
    void Update(double dt);
    void Render(double alpha);

    std::unique_ptr<Platform::Window> m_Window;
    std::unique_ptr<Platform::Input> m_Input;
    std::unique_ptr<Platform::RenderDevice> m_RenderDevice;
    std::unique_ptr<flecs::world> m_World;
    std::unique_ptr<Core::EventBus> m_EventBus;
    
    Core::GameContext m_Context;
    
    // Systems
    std::unique_ptr<Systems::AbilitySystem> m_AbilitySystem;
    std::unique_ptr<Systems::RenderSystem> m_RenderSystem;
    std::unique_ptr<Systems::PhysicsSystem> m_PhysicsSystem;
    std::unique_ptr<Systems::ScriptSystem> m_ScriptSystem;
    std::unique_ptr<Systems::EditorSystem> m_EditorSystem;

    bool m_IsRunning = false;
    bool m_EditorMode = true;
    double m_TimeLimit = 0.0;
};
