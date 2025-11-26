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
    std::shared_ptr<Resources::Animation> animation;
    float time = 0.0f;
    bool loop = true;
    
    // Runtime buffers
    ozz::vector<ozz::math::SoaTransform> locals;
    ozz::vector<ozz::math::Float4x4> models; // Skinning matrices
    std::unique_ptr<ozz::animation::SamplingJob::Context> context;
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
};
