#include "Engine.h"
#include "GamePlaySystem.h"
#include "GameComponents.h"
#include "Core/Log.h"
#include "Components/Components.h"
#include "Animation/AnimGraph.h"
#include "Resources/Texture.h"
#include "Resources/Mesh.h"
#include "Resources/Skeleton.h"
#include "Resources/Animation.h"
#include <memory>
#include <filesystem>

// Global state to keep alive between reloads if needed, 
// but for now we just re-init.
std::unique_ptr<GamePlaySystem> g_GamePlaySystem;
std::shared_ptr<Resources::Texture> g_TestTexture;
std::shared_ptr<Resources::Mesh> g_TestMesh;
std::shared_ptr<Resources::Skeleton> g_TestSkeleton;
std::shared_ptr<Resources::Animation> g_IdleAnimation;
std::shared_ptr<Resources::Animation> g_RunAnimation;

GAME_EXPORT void GameInit(Engine& engine) {
    LOG_INFO("GameInit: Initializing Sandbox Game Module");

    // Register Game Components
    engine.GetContext().World->component<AttributeSet>();

    // Load Test Texture
    // Path is relative to the executable (Build/Game/Sandbox/Debug/Sandbox.exe)
    // Assets are in Build/Game/Sandbox/Cooked/Assets/
    std::string assetPath = "../Cooked/Assets/test.oaktex";
    if (std::filesystem::exists(assetPath)) {
        g_TestTexture = engine.GetResourceManager().LoadTexture(assetPath);
        if (g_TestTexture) {
            LOG_INFO("Successfully loaded test texture: {}", assetPath);
            LOG_INFO("Texture Size: {}x{}", g_TestTexture->GetWidth(), g_TestTexture->GetHeight());
        } else {
            LOG_ERROR("Failed to load texture: {}", assetPath);
        }
    } else {
        LOG_WARN("Asset not found at: {}", assetPath);
        // Try absolute path for debugging if needed, or just warn.
    }

    // Load Test Mesh
    std::string meshPath = "../Cooked/Assets/Models/Joli.oakmesh";
    if (std::filesystem::exists(meshPath)) {
        g_TestMesh = engine.GetResourceManager().LoadMesh(meshPath);
        if (g_TestMesh) {
            LOG_INFO("Successfully loaded test mesh: {}", meshPath);
        } else {
            LOG_ERROR("Failed to load mesh: {}", meshPath);
        }
    }

    // Load Test Skeleton
    std::string skelPath = "../Cooked/Assets/Models/Joli.oakskel";
    if (std::filesystem::exists(skelPath)) {
        g_TestSkeleton = engine.GetResourceManager().LoadSkeleton(skelPath);
        if (g_TestSkeleton) {
            LOG_INFO("Successfully loaded test skeleton: {}", skelPath);
        } else {
            LOG_ERROR("Failed to load skeleton: {}", skelPath);
        }
    }

    // Load Idle Animation
    std::string idleAnimPath = "../Cooked/Assets/Models/Joli.oakanim";
    if (std::filesystem::exists(idleAnimPath)) {
        g_IdleAnimation = engine.GetResourceManager().LoadAnimation(idleAnimPath);
        if (g_IdleAnimation) {
            LOG_INFO("Successfully loaded idle animation: {}", idleAnimPath);
        } else {
            LOG_ERROR("Failed to load animation: {}", idleAnimPath);
        }
    }

    // Load Run Animation
    std::string runAnimPath = "../Cooked/Assets/Models/Joli_Run.oakanim";
    if (std::filesystem::exists(runAnimPath)) {
        g_RunAnimation = engine.GetResourceManager().LoadAnimation(runAnimPath);
        if (g_RunAnimation) {
            LOG_INFO("Successfully loaded run animation: {}", runAnimPath);
        } else {
            LOG_ERROR("Failed to load run animation: {}", runAnimPath);
        }
    }

    // Initialize Game Systems
    g_GamePlaySystem = std::make_unique<GamePlaySystem>(engine.GetContext());
    g_GamePlaySystem->Init();

    // Create test mesh entity first (so we can reference it for camera follow)
    flecs::entity meshEntity;
    if (engine.GetContext().World->count<MeshComponent>() == 0 && g_TestMesh) {
        meshEntity = engine.GetContext().World->entity("TestMesh")
            .set<LocalTransform>({ {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.01f, 0.01f, 0.01f} })
            .set<MeshComponent>({g_TestMesh})
            .set<CharacterController>({ 
                {0.0f, 0.0f, 0.0f},  // velocity
                3.0f,                 // moveSpeed (scaled for small character)
                2.0f,                 // runMultiplier
                10.0f,                // turnSpeed
                0.0f,                 // targetYaw
                CharacterState::Idle, // state
                true                  // isGrounded
            });
         
        if (g_TestSkeleton && g_IdleAnimation) {
            // Create AnimGraph for character
            auto animGraph = std::make_shared<Animation::AnimGraph>();
            
            // Add states with proper animations
            animGraph->AddState("Idle", g_IdleAnimation, 1.0f, true);
            animGraph->AddState("Walk", g_RunAnimation ? g_RunAnimation : g_IdleAnimation, 0.7f, true);  // Run anim at slower speed for walk
            animGraph->AddState("Run", g_RunAnimation ? g_RunAnimation : g_IdleAnimation, 1.0f, true);   // Run anim at full speed
            animGraph->SetDefaultState("Idle");
            
            // Add parameters
            animGraph->AddParameterBool("IsMoving", false);
            animGraph->AddParameterBool("IsRunning", false);
            animGraph->AddParameter("Speed", Animation::AnimParameter::Type::Float, 0.0f);
            
            // Add transitions
            // Idle -> Walk (when moving)
            animGraph->AddTransition("Idle", "Walk", 0.2f);
            animGraph->AddTransitionConditionBool("Idle", "Walk", "IsMoving", true);
            
            // Walk -> Idle (when stopped)
            animGraph->AddTransition("Walk", "Idle", 0.2f);
            animGraph->AddTransitionConditionBool("Walk", "Idle", "IsMoving", false);
            
            // Walk -> Run (when running)
            animGraph->AddTransition("Walk", "Run", 0.15f);
            animGraph->AddTransitionConditionBool("Walk", "Run", "IsRunning", true);
            
            // Run -> Walk (when not running)
            animGraph->AddTransition("Run", "Walk", 0.2f);
            animGraph->AddTransitionConditionBool("Run", "Walk", "IsRunning", false);
            
            // Run -> Idle (direct stop)
            animGraph->AddTransition("Run", "Idle", 0.3f);
            animGraph->AddTransitionConditionBool("Run", "Idle", "IsMoving", false);
            
            // Create animator with AnimGraph
            AnimatorComponent animator;
            animator.skeleton = g_TestSkeleton;
            animator.animGraph = animGraph;
            animator.graphInstance.Init(animGraph.get());
            
            meshEntity.set<AnimatorComponent>(std::move(animator));
            LOG_INFO("Added AnimatorComponent with AnimGraph to TestMesh entity");
        }
        LOG_INFO("Created TestMesh entity with MeshComponent and CharacterController");
    } else {
        meshEntity = engine.GetContext().World->lookup("TestMesh");
    }

    // Create Camera with third-person follow
    if (engine.GetContext().World->count<CameraComponent>() == 0) {
        auto cameraEntity = engine.GetContext().World->entity("MainCamera")
            .set<LocalTransform>({ {0.0f, 1.0f, 4.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} })
            .set<CameraComponent>({ 45.0f, 0.1f, 1000.0f, true });
        
        // Add CameraFollowComponent to orbit around the character
        if (meshEntity.is_valid()) {
            cameraEntity.set<CameraFollowComponent>({
                meshEntity,     // target
                5.0f,           // distance
                2.0f,           // minDistance
                20.0f,          // maxDistance
                0.0f,           // yaw
                20.0f,          // pitch
                -80.0f,         // minPitch
                80.0f,          // maxPitch
                {0.0f, 0.8f, 0.0f}, // offset (look at character center, scaled)
                0.2f,           // sensitivity
                1.0f            // zoomSpeed
            });
            LOG_INFO("Created MainCamera with third-person follow on TestMesh");
        } else {
            LOG_INFO("Created MainCamera entity (free-flight mode)");
        }
    }

    // Create Directional Light (sun)
    if (engine.GetContext().World->count<DirectionalLight>() == 0) {
        engine.GetContext().World->entity("Sun")
            .set<DirectionalLight>({
                glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f)),  // direction (pointing down and to the side)
                {1.0f, 0.95f, 0.9f},    // warm sunlight color
                1.0f,                    // intensity
                {0.15f, 0.15f, 0.2f}    // ambient (slight blue tint)
            });
        LOG_INFO("Created Sun directional light");
        
        // Add a point light for extra illumination
        engine.GetContext().World->entity("PointLight1")
            .set<LocalTransform>({ {2.0f, 2.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} })
            .set<PointLight>({
                {0.8f, 0.6f, 1.0f},    // purple-ish color
                2.0f,                   // intensity
                8.0f,                   // radius
                2.0f                    // falloff
            });
        LOG_INFO("Created PointLight1");
    }

    // Create a test entity with AttributeSet if it doesn't exist
    if (engine.GetContext().World->count<AttributeSet>() == 0) {
        engine.GetContext().World->entity("Player")
            .set<AttributeSet>({100.0f, 100.0f, 100.0f, 100.0f, 10.0f});

        // Add a static cube for testing
        std::string cubePath = "../Cooked/Assets/Models/cube.oakmesh";
        if (std::filesystem::exists(cubePath)) {
            auto cubeMesh = engine.GetResourceManager().LoadMesh(cubePath);
            if (cubeMesh) {
                engine.GetContext().World->entity("Cube")
                    .set<LocalTransform>({ {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} })
                    .set<MeshComponent>({cubeMesh});
                LOG_INFO("Created Cube entity");
            }
        }
    } else {
        // If entity exists (reloaded), update texture
        auto e = engine.GetContext().World->lookup("Player");
        if (e && g_TestTexture) {
            e.set<SpriteComponent>({g_TestTexture, {1,1,1,1}});
            LOG_INFO("Updated SpriteComponent on Player entity");
        }
    }
}

GAME_EXPORT void GameShutdown(Engine& engine) {
    LOG_INFO("GameShutdown: Unloading Sandbox Game Module");
    g_GamePlaySystem.reset();
    // We don't unregister components usually as that clears data
}
