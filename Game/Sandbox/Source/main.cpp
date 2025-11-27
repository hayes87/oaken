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
#include "Resources/ResourceManager.h"
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

// Test level meshes
std::shared_ptr<Resources::Mesh> g_GroundMesh;
std::shared_ptr<Resources::Mesh> g_CubeMesh;

// Helper to create a ground plane mesh
std::shared_ptr<Resources::Mesh> CreateGroundPlane(Resources::ResourceManager& rm, float size, int subdivisions) {
    std::vector<Resources::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float halfSize = size * 0.5f;
    float step = size / subdivisions;
    
    // Create grid of vertices
    for (int z = 0; z <= subdivisions; z++) {
        for (int x = 0; x <= subdivisions; x++) {
            Resources::Vertex v;
            v.position = glm::vec3(-halfSize + x * step, 0.0f, -halfSize + z * step);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2(static_cast<float>(x) / subdivisions, static_cast<float>(z) / subdivisions);
            v.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            v.joints = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            vertices.push_back(v);
        }
    }
    
    // Create triangles
    for (int z = 0; z < subdivisions; z++) {
        for (int x = 0; x < subdivisions; x++) {
            uint32_t topLeft = z * (subdivisions + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * (subdivisions + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;
            
            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    return rm.CreatePrimitiveMesh("ground_" + std::to_string(static_cast<int>(size)), vertices, indices);
}

// Helper to create a cube mesh
std::shared_ptr<Resources::Mesh> CreateCube(Resources::ResourceManager& rm, float size) {
    float h = size * 0.5f;
    
    std::vector<Resources::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Face data: position offsets and normal for each face
    struct Face { glm::vec3 normal; glm::vec3 right; glm::vec3 up; };
    Face faces[] = {
        { {0, 0, 1}, {1, 0, 0}, {0, 1, 0} },   // Front
        { {0, 0, -1}, {-1, 0, 0}, {0, 1, 0} }, // Back
        { {1, 0, 0}, {0, 0, -1}, {0, 1, 0} },  // Right
        { {-1, 0, 0}, {0, 0, 1}, {0, 1, 0} },  // Left
        { {0, 1, 0}, {1, 0, 0}, {0, 0, -1} },  // Top
        { {0, -1, 0}, {1, 0, 0}, {0, 0, 1} },  // Bottom
    };
    
    for (const auto& face : faces) {
        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
        
        // 4 corners of the face
        glm::vec3 center = face.normal * h;
        glm::vec3 corners[4] = {
            center - face.right * h - face.up * h,
            center + face.right * h - face.up * h,
            center + face.right * h + face.up * h,
            center - face.right * h + face.up * h,
        };
        glm::vec2 uvs[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
        
        for (int i = 0; i < 4; i++) {
            Resources::Vertex v;
            v.position = corners[i];
            v.normal = face.normal;
            v.uv = uvs[i];
            v.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            v.joints = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            vertices.push_back(v);
        }
        
        // Two triangles per face
        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    }
    
    return rm.CreatePrimitiveMesh("cube_" + std::to_string(static_cast<int>(size * 100)), vertices, indices);
}

GAME_EXPORT void GameInit(Engine& engine) {
    LOG_INFO("GameInit: Initializing Sandbox Game Module");

    // Register Game Components
    engine.GetContext().World->component<AttributeSet>();

    // Load Test Texture
    // Path is relative to the executable (Build/Game/Sandbox/Debug/Sandbox.exe)
    // Assets are copied to Build/Game/Sandbox/Debug/Assets/
    std::string assetPath = "Assets/test.oaktex";
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
    std::string meshPath = "Assets/Models/Joli.oakmesh";
    if (std::filesystem::exists(meshPath)) {
        g_TestMesh = engine.GetResourceManager().LoadMesh(meshPath);
        if (g_TestMesh) {
            LOG_INFO("Successfully loaded test mesh: {}", meshPath);
        } else {
            LOG_ERROR("Failed to load mesh: {}", meshPath);
        }
    }

    // Load Test Skeleton
    std::string skelPath = "Assets/Models/Joli.oakskel";
    if (std::filesystem::exists(skelPath)) {
        g_TestSkeleton = engine.GetResourceManager().LoadSkeleton(skelPath);
        if (g_TestSkeleton) {
            LOG_INFO("Successfully loaded test skeleton: {}", skelPath);
        } else {
            LOG_ERROR("Failed to load skeleton: {}", skelPath);
        }
    }

    // Load Idle Animation
    std::string idleAnimPath = "Assets/Models/Joli.oakanim";
    if (std::filesystem::exists(idleAnimPath)) {
        g_IdleAnimation = engine.GetResourceManager().LoadAnimation(idleAnimPath);
        if (g_IdleAnimation) {
            LOG_INFO("Successfully loaded idle animation: {}", idleAnimPath);
        } else {
            LOG_ERROR("Failed to load animation: {}", idleAnimPath);
        }
    }

    // Load Run Animation
    std::string runAnimPath = "Assets/Models/Joli_Run.oakanim";
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
        // Character spawns at Y=0 (ground collision is at Y=0)
        // The mesh is pre-scaled during cooking (cm to meters), so scale is 1.0
        // Mesh origin is at hip, so we offset it down to align feet with transform position
        // Capsule center height = radius + height/2 = 0.3 + 0.6 = 0.9m
        meshEntity = engine.GetContext().World->entity("TestMesh")
            .set<LocalTransform>({ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} })
            .set<MeshComponent>({g_TestMesh, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -0.9f, 0.0f}})  // Offset mesh down to align feet (hip is at center of capsule at 0.9m)
            .set<CharacterController>({ 
                {0.0f, 0.0f, 0.0f},  // velocity
                2.0f,                 // moveSpeed
                1.2f,                 // runMultiplier
                10.0f,                // turnSpeed
                0.0f,                 // targetYaw
                CharacterState::Idle, // state
                true                  // isGrounded
            })
            .set<CharacterPhysics>({
                1.2f,                 // height (world scale - 1.8 meters tall)
                0.3f,                 // radius (world scale - 0.3 meters)aaaaaaaaaaa
                70.0f,                // mass
                45.0f,                // max slope angle
                0.35f,                // max step height (35cm - can step over small objects)
                false,                // isOnGround (set by physics)
                {0.0f, 1.0f, 0.0f},   // ground normal
                nullptr               // characterVirtual (set by physics)
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
                {0.0f, 1.2f, 0.0f}, // offset (look at character chest height)
                0.2f,           // sensitivity
                1.0f,           // zoomSpeed
                0.85f,          // positionSmoothing (camera lag - higher = more lag)
                {0.0f, 0.0f, 0.0f}  // currentLookAt (runtime, starts at origin)
            });
            LOG_INFO("Created MainCamera with third-person follow on TestMesh");
        } else {
            LOG_INFO("Created MainCamera entity (free-flight mode)");
        }
    }

    // ============================================
    // CREATE TEST LEVEL GEOMETRY WITH PHYSICS
    // ============================================
    
    // Create ground plane with physics
    g_GroundMesh = CreateGroundPlane(engine.GetResourceManager(), 100.0f, 20);
    if (g_GroundMesh) {
        engine.GetContext().World->entity("Ground")
            .set<LocalTransform>({ {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} })
            .set<MeshComponent>({g_GroundMesh})
            .set<Collider>({
                ColliderType::Box,
                {50.0f, 0.5f, 50.0f},  // Half extents (100x1x100 box)
                {0.0f, -0.5f, 0.0f},   // Offset down so top is at Y=0
                0,                      // Layer
                false                   // Not a trigger
            })
            .set<RigidBody>({
                MotionType::Static,
                1.0f, 0.5f, 0.0f, 0.05f, 0.05f,
                {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
                false, false, false,
                0xFFFFFFFF
            });
        LOG_INFO("Created Ground plane with physics");
    }
    
    // Create procedural cube mesh for obstacles
    g_CubeMesh = CreateCube(engine.GetResourceManager(), 1.0f);
    if (g_CubeMesh) {
        // Create a variety of obstacles around the level
        struct ObstacleInfo { glm::vec3 pos; glm::vec3 scale; float rotY; };
        std::vector<ObstacleInfo> obstacles = {
            // Central area - some scattered cubes
            { {5.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 1.0f}, 0.0f },
            { {-4.0f, 0.0f, 3.0f}, {1.5f, 1.0f, 1.5f}, 45.0f },
            { {3.0f, 0.0f, -5.0f}, {0.8f, 3.0f, 0.8f}, 30.0f },
            { {-6.0f, 0.0f, -4.0f}, {2.0f, 0.5f, 2.0f}, 0.0f },
            
            // Platform / staircase area
            { {10.0f, 0.0f, 10.0f}, {3.0f, 0.3f, 3.0f}, 0.0f },
            { {10.0f, 0.6f, 10.0f}, {2.0f, 0.3f, 2.0f}, 0.0f },
            { {10.0f, 1.2f, 10.0f}, {1.0f, 0.3f, 1.0f}, 0.0f },
            
            // Wall section
            { {-10.0f, 0.0f, 0.0f}, {0.5f, 2.5f, 8.0f}, 0.0f },
            
            // Scattered pillars
            { {15.0f, 0.0f, -8.0f}, {1.0f, 4.0f, 1.0f}, 0.0f },
            { {-12.0f, 0.0f, 12.0f}, {1.0f, 4.0f, 1.0f}, 0.0f },
            { {8.0f, 0.0f, -15.0f}, {1.0f, 4.0f, 1.0f}, 0.0f },
            { {-15.0f, 0.0f, -10.0f}, {1.0f, 4.0f, 1.0f}, 0.0f },
            
            // Ramp-like structures
            { {20.0f, 0.0f, 5.0f}, {4.0f, 0.5f, 2.0f}, 15.0f },
            { {-20.0f, 0.0f, -5.0f}, {4.0f, 0.5f, 2.0f}, -20.0f },
            
            // Maze-like corner
            { {-20.0f, 0.0f, 15.0f}, {0.5f, 1.5f, 5.0f}, 0.0f },
            { {-17.0f, 0.0f, 18.0f}, {5.0f, 1.5f, 0.5f}, 0.0f },
            { {-14.0f, 0.0f, 15.0f}, {0.5f, 1.5f, 5.0f}, 0.0f },
        };
        
        int obstacleIdx = 0;
        for (const auto& obs : obstacles) {
            std::string name = "Obstacle_" + std::to_string(obstacleIdx++);
            float yPos = obs.pos.y + obs.scale.y * 0.5f - 1.0f;
            engine.GetContext().World->entity(name.c_str())
                .set<LocalTransform>({ 
                    {obs.pos.x, yPos, obs.pos.z},
                    {0.0f, obs.rotY, 0.0f}, 
                    obs.scale 
                })
                .set<MeshComponent>({g_CubeMesh})
                .set<Collider>({
                    ColliderType::Box,
                    {obs.scale.x * 0.5f, obs.scale.y * 0.5f, obs.scale.z * 0.5f},  // Half extents
                    {0.0f, 0.0f, 0.0f},  // No offset (mesh is centered)
                    0,
                    false
                })
                .set<RigidBody>({
                    MotionType::Static,
                    1.0f, 0.5f, 0.0f, 0.05f, 0.05f,
                    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
                    false, false, false,
                    0xFFFFFFFF
                });
        }
        LOG_INFO("Created {} obstacle entities with physics", obstacles.size());
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
    }
}

GAME_EXPORT void GameShutdown(Engine& engine) {
    LOG_INFO("GameShutdown: Unloading Sandbox Game Module");
    g_GamePlaySystem.reset();
    g_GroundMesh.reset();
    g_CubeMesh.reset();
    // We don't unregister components usually as that clears data
}
