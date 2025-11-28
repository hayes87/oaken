#include "Engine.h"
#include "Core/Profiler.h"
#include "Core/TimeStep.h"
#include "Core/Log.h"
#include "Components/Components.h"
#include "Components/Reflection.h"
#include "Scene/SceneSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

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

    // Initialize ImGui before RenderSystem
    InitImGui();

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
    m_CharacterSystem = std::make_unique<Systems::CharacterSystem>(*m_Context.World, *m_Input);
    
    // Map some test input
    m_Input->MapAction("Cast_Slot_1"_hs, SDL_SCANCODE_SPACE);
    
    // Load config file (overrides defaults)
    LoadConfig();
    
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
            // Let ImGui process events first
            ImGui_ImplSDL3_ProcessEvent(&event);
            
            m_Input->ProcessEvent(event);
            if (event.type == SDL_EVENT_QUIT) {
                m_Window->SetShouldClose(true);
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                m_Window->OnResize(event.window.data1, event.window.data2);
            }
        }
        
        // Handle debug keyboard toggles
        HandleDebugInput();
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
        
        // Update FPS counter
        UpdateFPSCounter(static_cast<float>(frameTime));
        
        // Start new ImGui frame
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        
        // Render debug menu (creates ImGui draw commands)
        RenderDebugMenu();
        
        // Finalize ImGui draw data
        ImGui::Render();
        
        // Start the render system frame (acquires command buffer)
        m_RenderSystem->BeginFrame(m_ShowSkeleton);
        
        // Prepare ImGui draw data (uploads vertex/index buffers) - must be before render pass
        Imgui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), m_RenderDevice->GetCommandBuffer());
        
        // Debug: Draw physics colliders (must be before DrawScene so lines get rendered)
        if (m_ShowColliders) {
            m_RenderSystem->DrawPhysicsDebug();
        }
        
        // Draw the scene (this starts and uses the render pass)
        m_RenderSystem->DrawScene(alpha);
        
        if (m_EditorMode) {
            m_EditorSystem->DrawUI(m_Context.World);
        }
        
        // Run bloom + tone mapping (renders to swapchain, leaves pass open for UI)
        m_RenderSystem->EndFrame();
        
        // Render ImGui AFTER tone mapping so it doesn't get bloomed
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), 
                                          m_RenderDevice->GetCommandBuffer(),
                                          m_RenderDevice->GetRenderPass());
        
        // End render pass and submit command buffer
        m_RenderSystem->FinishFrame();
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
    // 1. Shutdown ImGui
    ShutdownImGui();

    // 2. Destroy Systems (Release references to World/Context)
    m_AbilitySystem.reset();
    m_PhysicsSystem.reset();
    m_ScriptSystem.reset();
    m_EditorSystem.reset();
    m_TransformSystem.reset();
    m_RenderSystem.reset();

    // 3. Destroy SceneManager (Destroys Scene and World)
    m_SceneManager.reset();
    m_Context.World = nullptr;

    // 4. Destroy ResourceManager (Releases cached Textures)
    m_ResourceManager.reset();

    // 5. Destroy RenderDevice (Destroys GPU Device)
    m_RenderDevice.reset();

    // 6. Destroy Window
    m_Window.reset();
}

void Engine::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    ImGui::StyleColorsDark();
    
    // Make the style a bit nicer
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    
    ImGui_ImplSDL3_InitForSDLGPU(m_Window->GetNativeWindow());
    
    ImGui_ImplSDLGPU3_InitInfo initInfo = {};
    initInfo.Device = m_RenderDevice->GetDevice();
    initInfo.ColorTargetFormat = m_RenderDevice->GetSwapchainTextureFormat();
    initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&initInfo);
}

void Engine::ShutdownImGui() {
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void Engine::HandleDebugInput() {
    // F11 toggles debug menu
    if (m_Input->WasKeyPressed(SDL_SCANCODE_F11)) {
        m_ShowDebugMenu = !m_ShowDebugMenu;
        LOG_CORE_INFO("Debug menu: {}", m_ShowDebugMenu ? "ON" : "OFF");
        
        // Toggle mouse lock - unlock when debug menu opens, lock when it closes
        m_Input->SetRelativeMouseMode(!m_ShowDebugMenu);
    }
    // F1 toggles colliders
    if (m_Input->WasKeyPressed(SDL_SCANCODE_F1)) {
        m_ShowColliders = !m_ShowColliders;
        LOG_CORE_INFO("Colliders: {}", m_ShowColliders ? "ON" : "OFF");
    }
    // F2 toggles skeleton
    if (m_Input->WasKeyPressed(SDL_SCANCODE_F2)) {
        m_ShowSkeleton = !m_ShowSkeleton;
        LOG_CORE_INFO("Skeleton: {}", m_ShowSkeleton ? "ON" : "OFF");
    }
}

void Engine::UpdateFPSCounter(float deltaTime) {
    m_FrameCount++;
    m_FPSAccumulator += deltaTime;
    
    if (m_FPSAccumulator >= 1.0f) {
        m_CurrentFPS = static_cast<float>(m_FrameCount) / m_FPSAccumulator;
        m_FrameCount = 0;
        m_FPSAccumulator = 0.0f;
    }
}

void Engine::RenderDebugMenu() {
    if (!m_ShowDebugMenu) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Debug Menu (F11)", &m_ShowDebugMenu)) {
        // FPS display
        if (m_ShowFPS) {
            ImGui::Text("FPS: %.1f (%.3f ms)", m_CurrentFPS, 1000.0f / m_CurrentFPS);
            ImGui::Separator();
        }
        
        // Render stats
        if (m_RenderSystem) {
            const auto& stats = m_RenderSystem->GetStats();
            ImGui::Text("Draw Calls: %u", stats.drawCalls);
            ImGui::Text("Total Instances: %u", stats.totalInstances);
            ImGui::Text("Batched: %u | Skinned: %u", stats.batchedInstances, stats.skinnedInstances);
            ImGui::Separator();
        }
        
        // Rendering options
        if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show FPS", &m_ShowFPS);
            ImGui::Checkbox("Show Colliders (F1)", &m_ShowColliders);
            ImGui::Checkbox("Show Skeleton (F2)", &m_ShowSkeleton);
        }
        
        // HDR / Tone Mapping options
        if (ImGui::CollapsingHeader("HDR & Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool hdrEnabled = m_RenderDevice->IsHDREnabled();
            if (ImGui::Checkbox("HDR Enabled", &hdrEnabled)) {
                // Note: Changing HDR at runtime requires pipeline recreation, so this is read-only for now
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("HDR is configured at startup. Restart to change.");
            }
            
            float exposure = m_RenderDevice->GetExposure();
            if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f, "%.2f")) {
                m_RenderDevice->SetExposure(exposure);
            }
            
            float gamma = m_RenderDevice->GetGamma();
            if (ImGui::SliderFloat("Gamma", &gamma, 1.0f, 3.0f, "%.2f")) {
                m_RenderDevice->SetGamma(gamma);
            }
            
            const char* tonemapNames[] = { "Reinhard", "ACES", "Uncharted 2" };
            int currentTonemap = static_cast<int>(m_RenderDevice->GetToneMapOperator());
            if (ImGui::Combo("Tone Map", &currentTonemap, tonemapNames, 3)) {
                m_RenderDevice->SetToneMapOperator(static_cast<Platform::ToneMapOperator>(currentTonemap));
            }
            
            ImGui::Separator();
            ImGui::Text("Bloom");
            
            bool bloomEnabled = m_RenderDevice->IsBloomEnabled();
            if (ImGui::Checkbox("Bloom Enabled", &bloomEnabled)) {
                m_RenderDevice->SetBloomEnabled(bloomEnabled);
            }
            
            float bloomThreshold = m_RenderDevice->GetBloomThreshold();
            if (ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.0f, 2.0f, "%.2f")) {
                m_RenderDevice->SetBloomThreshold(bloomThreshold);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Brightness threshold for bloom extraction. Lower = more bloom.");
            }
            
            float bloomIntensity = m_RenderDevice->GetBloomIntensity();
            if (ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0.0f, 3.0f, "%.2f")) {
                m_RenderDevice->SetBloomIntensity(bloomIntensity);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Strength of the bloom effect. >1.5 = debug mode (shows bloom texture only)");
            }
            
            int bloomPasses = m_RenderDevice->GetBloomBlurPasses();
            if (ImGui::SliderInt("Blur Passes", &bloomPasses, 1, 10)) {
                m_RenderDevice->SetBloomBlurPasses(bloomPasses);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Number of blur iterations. More = softer bloom, but slower.");
            }
            
            ImGui::Separator();
            ImGui::Text("Shadows");
            
            bool shadowsEnabled = m_RenderDevice->IsShadowsEnabled();
            if (ImGui::Checkbox("Shadows Enabled", &shadowsEnabled)) {
                m_RenderDevice->SetShadowsEnabled(shadowsEnabled);
            }
            
            float shadowBias = m_RenderDevice->GetShadowBias();
            if (ImGui::SliderFloat("Shadow Bias", &shadowBias, 0.0f, 0.01f, "%.5f")) {
                m_RenderDevice->SetShadowBias(shadowBias);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Depth bias to prevent shadow acne. Too high = peter panning.");
            }
            
            float shadowNormalBias = m_RenderDevice->GetShadowNormalBias();
            if (ImGui::SliderFloat("Normal Bias", &shadowNormalBias, 0.0f, 0.1f, "%.3f")) {
                m_RenderDevice->SetShadowNormalBias(shadowNormalBias);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Normal-based bias for grazing angles.");
            }
            
            int pcfSamples = m_RenderDevice->GetShadowPcfSamples();
            if (ImGui::SliderInt("PCF Samples", &pcfSamples, 0, 4)) {
                m_RenderDevice->SetShadowPcfSamples(pcfSamples);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("0=hard shadows, 1=3x3 PCF, 2=5x5 PCF, etc.");
            }
        }
        
        // Engine info
        if (ImGui::CollapsingHeader("Engine Info")) {
            ImGui::Text("Total Time: %.2f s", m_TotalTime);
            if (m_TimeLimit > 0.0) {
                ImGui::Text("Time Limit: %.2f s", m_TimeLimit);
                float progress = static_cast<float>(m_TotalTime / m_TimeLimit);
                ImGui::ProgressBar(progress, ImVec2(-1, 0));
            }
        }
        
        // Keyboard shortcuts help
        if (ImGui::CollapsingHeader("Shortcuts")) {
            ImGui::BulletText("F11 - Toggle Debug Menu");
            ImGui::BulletText("F1 - Toggle Colliders");
            ImGui::BulletText("F2 - Toggle Skeleton");
            ImGui::BulletText("WASD - Move");
            ImGui::BulletText("Mouse - Look");
            ImGui::BulletText("Shift - Sprint");
            ImGui::BulletText("Space - Jump");
        }
        
        // Config save/load
        ImGui::Separator();
        if (ImGui::Button("Save Config")) {
            SaveConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Config")) {
            LoadConfig();
        }
    }
    ImGui::End();
}

void Engine::LoadConfig() {
    std::ifstream file("engine.json");
    if (!file.is_open()) {
        LOG_CORE_WARN("No config file found (engine.json), using defaults");
        return;
    }
    
    try {
        json config;
        file >> config;
        
        // HDR settings
        if (config.contains("hdr")) {
            auto& hdr = config["hdr"];
            if (hdr.contains("exposure")) m_RenderDevice->SetExposure(hdr["exposure"]);
            if (hdr.contains("gamma")) m_RenderDevice->SetGamma(hdr["gamma"]);
            if (hdr.contains("tonemapOperator")) {
                m_RenderDevice->SetToneMapOperator(static_cast<Platform::ToneMapOperator>(hdr["tonemapOperator"].get<int>()));
            }
        }
        
        // Bloom settings
        if (config.contains("bloom")) {
            auto& bloom = config["bloom"];
            if (bloom.contains("enabled")) m_RenderDevice->SetBloomEnabled(bloom["enabled"]);
            if (bloom.contains("threshold")) m_RenderDevice->SetBloomThreshold(bloom["threshold"]);
            if (bloom.contains("intensity")) m_RenderDevice->SetBloomIntensity(bloom["intensity"]);
            if (bloom.contains("blurPasses")) m_RenderDevice->SetBloomBlurPasses(bloom["blurPasses"]);
        }
        
        // Debug settings
        if (config.contains("debug")) {
            auto& debug = config["debug"];
            if (debug.contains("showFPS")) m_ShowFPS = debug["showFPS"];
            if (debug.contains("showColliders")) m_ShowColliders = debug["showColliders"];
            if (debug.contains("showSkeleton")) m_ShowSkeleton = debug["showSkeleton"];
        }
        
        LOG_CORE_INFO("Config loaded from engine.json");
    } catch (const std::exception& e) {
        LOG_CORE_ERROR("Failed to parse config: {}", e.what());
    }
}

void Engine::SaveConfig() {
    json config;
    
    // HDR settings
    config["hdr"] = {
        {"exposure", m_RenderDevice->GetExposure()},
        {"gamma", m_RenderDevice->GetGamma()},
        {"tonemapOperator", static_cast<int>(m_RenderDevice->GetToneMapOperator())}
    };
    
    // Bloom settings
    config["bloom"] = {
        {"enabled", m_RenderDevice->IsBloomEnabled()},
        {"threshold", m_RenderDevice->GetBloomThreshold()},
        {"intensity", m_RenderDevice->GetBloomIntensity()},
        {"blurPasses", m_RenderDevice->GetBloomBlurPasses()}
    };
    
    // Debug settings
    config["debug"] = {
        {"showFPS", m_ShowFPS},
        {"showColliders", m_ShowColliders},
        {"showSkeleton", m_ShowSkeleton}
    };
    
    std::ofstream file("engine.json");
    if (file.is_open()) {
        file << config.dump(4);  // Pretty print with 4-space indent
        LOG_CORE_INFO("Config saved to engine.json");
    } else {
        LOG_CORE_ERROR("Failed to save config file");
    }
}
