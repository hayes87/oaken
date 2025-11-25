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
#include "Scene/SceneSerializer.h"
#include <iostream>

Engine::Engine() {
    Core::Log::Init();
    LOG_CORE_INFO("Initializing Oaken Engine...");

    m_EventBus = std::make_unique<Core::EventBus>();
    m_SceneManager = std::make_unique<Core::SceneManager>();
    m_Input = std::make_unique<Platform::Input>();
    m_ResourceManager = std::make_unique<Resources::ResourceManager>();
    
    // Setup Context
    // World will be set when a scene is loaded
    m_Context.World = nullptr;
    m_Context.Events = m_EventBus.get();

    // Create Systems
    m_AbilitySystem = std::make_unique<Systems::AbilitySystem>(m_Context);
    m_PhysicsSystem = std::make_unique<Systems::PhysicsSystem>(m_Context);
    m_ScriptSystem = std::make_unique<Systems::ScriptSystem>(m_Context);
    m_EditorSystem = std::make_unique<Systems::EditorSystem>(m_Context);
    m_TransformSystem = std::make_unique<Systems::TransformSystem>(m_Context);
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

    m_ResourceManager->Init(m_RenderDevice.get());

    m_RenderSystem = std::make_unique<Systems::RenderSystem>(m_Context, *m_RenderDevice, *m_ResourceManager);
    m_RenderSystem->Init();

    m_Input->Init(m_EventBus.get(), m_Window->GetNativeWindow());
    
    // Create and Load Scene
    auto scene = std::make_unique<Core::Scene>();
    
    Core::SceneSerializer serializer(scene.get(), m_ResourceManager.get());
    // Try loading from relative path (assuming running from Cooked folder)
    if (serializer.DeserializeBinary("Assets/Scenes/Test.oaklevel")) {
        LOG_CORE_INFO("Loaded Test.oaklevel");
    } else {
        LOG_CORE_WARN("Failed to load Test.oaklevel, creating default scene");
        auto e = scene->GetWorld().entity("Player")
            .set<LocalTransform>({ {0,0,0}, {0,0,0}, {1,1,1} });
    }

    m_SceneManager->LoadScene(std::move(scene));
    m_Context.World = &m_SceneManager->GetActiveScene()->GetWorld();

    // Init Systems
    m_AnimationSystem = std::make_unique<Systems::AnimationSystem>(*m_Context.World);
    m_AbilitySystem->Init();
    m_PhysicsSystem->Init();
    m_ScriptSystem->Init();
    m_EditorSystem->Init();
    m_TransformSystem->Init();
    m_CameraSystem = std::make_unique<Systems::CameraSystem>(*m_Context.World, *m_Input);
    
    // Map some test input
    m_Input->MapAction("Cast_Slot_1"_hs, SDL_SCANCODE_SPACE);
    
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
            m_EditorSystem->DrawUI(m_Context.World);
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
    m_ResourceManager->Update();
    
    // Run Flecs Systems (e.g. Animation)
    if (m_Context.World) {
        m_Context.World->progress(dt);
    }

    m_PhysicsSystem->Step(dt);
    m_AbilitySystem->TickCooldowns(dt);
    m_ScriptSystem->Update(dt);
    m_SceneManager->Update(dt);
}

void Engine::Render(double alpha) {
    (void)alpha;
    // Render logic moved to RenderSystem
}

void Engine::Shutdown() {
    // 1. Destroy Systems (Release references to World/Context)
    m_AbilitySystem.reset();
    m_PhysicsSystem.reset();
    m_ScriptSystem.reset();
    m_EditorSystem.reset();
    m_TransformSystem.reset();
    m_RenderSystem.reset();

    // 2. Destroy SceneManager (Destroys Scene and World)
    m_SceneManager.reset();
    m_Context.World = nullptr;

    // 3. Destroy ResourceManager (Releases cached Textures)
    m_ResourceManager.reset();

    // 4. Destroy RenderDevice (Destroys GPU Device)
    m_RenderDevice.reset();

    // 5. Destroy Window
    m_Window.reset();
}
