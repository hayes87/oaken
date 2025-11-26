#pragma once

#include <flecs.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/animation/runtime/sampling_job.h>
#include "../Resources/Skeleton.h"
#include "../Resources/Animation.h"
#include "../Animation/AnimGraph.h"

namespace Resources { 
    class Texture; 
    class Mesh;
}

struct LocalTransform {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f}; // Euler angles in degrees
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct WorldTransform {
    glm::mat4 matrix = glm::mat4(1.0f);
};

struct SpriteComponent {
    std::shared_ptr<Resources::Texture> texture;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct MeshComponent {
    std::shared_ptr<Resources::Mesh> mesh;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // Simple color for now
};

struct AnimatorComponent {
    std::shared_ptr<Resources::Skeleton> skeleton;
    std::shared_ptr<Resources::Animation> animation;  // Legacy: single animation
    float time = 0.0f;
    bool loop = true;
    
    // AnimGraph support (optional - if set, overrides single animation)
    std::shared_ptr<Animation::AnimGraph> animGraph;
    Animation::AnimGraphInstance graphInstance;
    
    // Runtime buffers
    ozz::vector<ozz::math::SoaTransform> locals;
    ozz::vector<ozz::math::SoaTransform> blendLocals;  // For blending
    ozz::vector<ozz::math::Float4x4> models; // Skinning matrices
    std::unique_ptr<ozz::animation::SamplingJob::Context> context;
    std::unique_ptr<ozz::animation::SamplingJob::Context> blendContext;  // For second animation during blend
};

struct ScriptComponent {
    std::string scriptName;
};

struct CameraComponent {
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool isPrimary = true;
    bool isOrthographic = false;
    float orthoSize = 10.0f; // Half-height in world units for ortho projection
};

// Third-person orbit camera that follows a target entity
struct CameraFollowComponent {
    flecs::entity target;       // Entity to follow
    float distance = 5.0f;      // Distance from target
    float minDistance = 2.0f;   // Minimum zoom distance
    float maxDistance = 20.0f;  // Maximum zoom distance
    float yaw = 0.0f;           // Horizontal rotation around target (degrees)
    float pitch = 20.0f;        // Vertical rotation around target (degrees)
    float minPitch = -80.0f;    // Minimum pitch angle
    float maxPitch = 80.0f;     // Maximum pitch angle
    glm::vec3 offset = {0.0f, 1.5f, 0.0f}; // Offset from target pivot (e.g., look at head not feet)
    float sensitivity = 0.2f;   // Mouse sensitivity
    float zoomSpeed = 1.0f;     // Scroll wheel zoom speed
    float positionSmoothing = 0.1f;  // Camera position lag (0 = instant, 1 = very slow)
    glm::vec3 currentLookAt = {0.0f, 0.0f, 0.0f}; // Current interpolated look-at position (runtime)
};

// Character movement states
enum class CharacterState {
    Idle,
    Walking,
    Running,
    Jumping,
    Falling
};

// Character controller for player/NPC movement
struct CharacterController {
    glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
    float moveSpeed = 5.0f;         // Units per second
    float runMultiplier = 2.0f;     // Speed multiplier when running
    float turnSpeed = 10.0f;        // Rotation speed (degrees per second)
    float targetYaw = 0.0f;         // Target rotation (character faces this direction)
    CharacterState state = CharacterState::Idle;
    bool isGrounded = true;         // For future physics integration
};

// Directional light (sun-like, affects entire scene)
struct DirectionalLight {
    glm::vec3 direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    glm::vec3 color = {1.0f, 0.95f, 0.9f}; // Warm sunlight
    float intensity = 1.0f;
    glm::vec3 ambient = {0.1f, 0.1f, 0.15f}; // Ambient contribution
};

// Point light (local light source with falloff)
struct PointLight {
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;           // Attenuation radius
    float falloff = 2.0f;           // Falloff exponent (2.0 = quadratic)
};

// ============================================
// PHYSICS COMPONENTS
// ============================================

// Collider shape types
enum class ColliderType {
    Box,
    Sphere,
    Capsule,
    Mesh        // For static geometry
};

// Motion types for rigid bodies
enum class MotionType {
    Static,     // Never moves (walls, floors)
    Kinematic,  // Moved by code, not physics (moving platforms)
    Dynamic     // Fully simulated by physics
};

// Collider component - defines the collision shape
struct Collider {
    ColliderType type = ColliderType::Box;
    glm::vec3 size = {1.0f, 1.0f, 1.0f};    // Box half-extents, or (radius, height, 0) for capsule
    glm::vec3 offset = {0.0f, 0.0f, 0.0f};  // Offset from entity origin
    uint16_t layer = 0;                      // Collision layer (for filtering)
    bool isTrigger = false;                  // If true, generates events but no physical response
};

// RigidBody component - physics simulation properties
struct RigidBody {
    MotionType motionType = MotionType::Dynamic;
    float mass = 1.0f;                       // Mass in kg (ignored for static/kinematic)
    float friction = 0.5f;                   // Surface friction
    float restitution = 0.0f;                // Bounciness (0 = no bounce, 1 = full bounce)
    float linearDamping = 0.05f;             // Air resistance for linear velocity
    float angularDamping = 0.05f;            // Air resistance for angular velocity
    glm::vec3 linearVelocity = {0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity = {0.0f, 0.0f, 0.0f};
    bool lockRotationX = false;
    bool lockRotationY = false;
    bool lockRotationZ = false;
    
    // Runtime - Jolt body ID (set by PhysicsSystem)
    uint32_t bodyId = 0xFFFFFFFF;            // Invalid ID by default
};

// Character physics component - uses Jolt's CharacterVirtual for responsive movement
struct CharacterPhysics {
    float height = 1.8f;                     // Character capsule height
    float radius = 0.3f;                     // Character capsule radius
    float mass = 70.0f;                      // Mass for push interactions
    float maxSlopeAngle = 45.0f;             // Max walkable slope in degrees
    float maxStepHeight = 0.3f;              // Max step height character can climb
    bool isOnGround = false;                 // Set by physics system
    glm::vec3 groundNormal = {0.0f, 1.0f, 0.0f};
    
    // Runtime - Jolt character pointer (set by PhysicsSystem)
    void* characterVirtual = nullptr;
};
