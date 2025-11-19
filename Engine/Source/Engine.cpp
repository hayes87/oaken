#include "Engine.h"
#include "Core/Profiler.h"
#include "Core/TimeStep.h"
#include <iostream>

#include "Components/Reflection.h"

#include "Engine.h"
#include "Core/Profiler.h"
#include "Core/TimeStep.h"
#include "Core/Log.h"
#include "Components/Components.h"
#include <iostream>

Engine::Engine() {
    Core::Log::Init();
    LOG_CORE_INFO("Initializing Oaken Engine...");

    m_EventBus = std::make_unique<Core::EventBus>();
    m_World = std::make_unique<flecs::world>();
    m_Input = std::make_unique<Platform::Input>();
    
    // Setup Context
    m_Context.World = m_World.get();
    m_Context.Events = m_EventBus.get();

    // Create Systems
    m_AbilitySystem = std::make_unique<Systems::AbilitySystem>(m_Context);
    m_PhysicsSystem = std::make_unique<Systems::PhysicsSystem>(m_Context);
    m_ScriptSystem = std::make_unique<Systems::ScriptSystem>(m_Context);
    m_EditorSystem = std::make_unique<Systems::EditorSystem>(m_Context);
    // RenderSystem needs RenderDevice, created in Init
}

Engine::~Engine() {
    Shutdown();
}

bool Engine::Init() {
    Platform::WindowProps props = { "Oaken Engine", 1280, 720 };
    m_Window = std::make_unique<Platform::Window>(props);
    
    if (!m_Window->Init()) {
        return false;
    }

    m_RenderDevice = std::make_unique<Platform::RenderDevice>();
    if (!m_RenderDevice->Init(m_Window.get())) {
        return false;
    }

    m_RenderSystem = std::make_unique<Systems::RenderSystem>(m_Context, *m_RenderDevice);
    m_RenderSystem->Init();

    m_Input->Init(m_EventBus.get());
    
    // Register Components
    Components::RegisterReflection(*m_World);

    // Init Systems
    m_AbilitySystem->Init();
    m_PhysicsSystem->Init();
    m_ScriptSystem->Init();
    m_EditorSystem->Init();
    
    // Map some test input
    m_Input->MapAction("Cast_Slot_1"_hs, SDL_SCANCODE_SPACE);
    
    // Create a test entity
    auto e = m_World->entity("Player")
        .set<Transform>({ {0,0,0}, {0,0,0}, {1,1,1} });
    
    // Initialize time
    m_CurrentTime = SDL_GetTicks() / 1000.0;
    m_Accumulator = 0.0;
    m_TotalTime = 0.0;
    m_IsRunning = true;

    return true;
}

bool Engine::Step() {
    if (!m_IsRunning || m_Window->ShouldClose()) {
        return false;
    }

    PROFILE_FRAME("MainLoop");

    uint64_t newTicks = SDL_GetTicks();
    double newTime = newTicks / 1000.0;
    double frameTime = newTime - m_CurrentTime;
    m_CurrentTime = newTime;
    
    // Prevent spiral of death
    if (frameTime > 0.25) frameTime = 0.25;
    
    m_Accumulator += frameTime;

    if (m_TimeLimit > 0.0 && m_TotalTime >= m_TimeLimit) {
        LOG_CORE_INFO("Time limit reached ({:.2f}s). Shutting down.", m_TimeLimit);
        m_IsRunning = false;
        return false;
    }

    // 1. Input
    {
        PROFILE_SCOPE("Input");
        m_Input->Poll();
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_Input->ProcessEvent(event);
            if (event.type == SDL_EVENT_QUIT) {
                m_Window->SetShouldClose(true);
            }
        }
    }

    // 2. Physics/Logic (Fixed Step)
    double dt = Core::TimeStep::FixedDeltaTime;
    {
        PROFILE_SCOPE("FixedUpdate");
        while (m_Accumulator >= dt) {
            Update(dt);
            m_Accumulator -= dt;
            m_TotalTime += dt;
        }
    }

    // 3. Render (Interpolated)
    double alpha = m_Accumulator / dt;
    {
        PROFILE_SCOPE("Render");
        m_RenderSystem->BeginFrame();
        m_RenderSystem->DrawScene(alpha);
        
        if (m_EditorMode) {
            m_EditorSystem->DrawUI(m_World.get());
        }
        
        m_RenderSystem->EndFrame();
    }

    return true;
}

void Engine::Run() {
    m_IsRunning = true;
    while (Step()) {}
}

void Engine::Update(double dt) {
    m_PhysicsSystem->Step(dt);
    m_AbilitySystem->TickCooldowns(dt);
    m_ScriptSystem->Update(dt);
    m_World->progress((float)dt);
}

void Engine::Render(double alpha) {
    (void)alpha;
    // Render logic moved to RenderSystem
}

void Engine::Shutdown() {
    if (m_RenderDevice) {
        m_RenderDevice->Shutdown();
    }
    if (m_Window) {
        m_Window->Shutdown();
    }
}
