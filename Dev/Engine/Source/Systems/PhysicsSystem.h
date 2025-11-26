#pragma once

#include "../Core/Context.h"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

// Forward declarations to avoid including Jolt headers in the header
namespace JPH {
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemSingleThreaded;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    class BodyInterface;
    class BodyActivationListener;
    class ContactListener;
    class CharacterVirtual;
}

namespace flecs {
    struct entity;
}

namespace Systems {

    // Collision layers
    namespace Layers {
        static constexpr uint16_t NON_MOVING = 0;
        static constexpr uint16_t MOVING = 1;
        static constexpr uint16_t CHARACTER = 2;
        static constexpr uint16_t NUM_LAYERS = 3;
    }

    // Broad phase layers  
    namespace BroadPhaseLayers {
        static constexpr uint8_t NON_MOVING = 0;
        static constexpr uint8_t MOVING = 1;
        static constexpr uint8_t NUM_LAYERS = 2;
    }

    class PhysicsSystem {
    public:
        PhysicsSystem(Core::GameContext& context);
        ~PhysicsSystem();
        
        void Init();
        void Shutdown();
        void Step(float dt);
        
        // Create physics bodies for entities with Collider + RigidBody
        void SyncBodiesToPhysics();
        
        // Update ECS transforms from physics simulation
        void SyncPhysicsToTransforms();
        
        // Character physics
        void UpdateCharacterPhysics(float dt);
        
        // Body management
        uint32_t CreateBody(flecs::entity entity);
        void DestroyBody(uint32_t bodyId);
        
        // Character management  
        void* CreateCharacter(flecs::entity entity);
        void DestroyCharacter(void* character);
        
        // Queries
        bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                    glm::vec3& outHitPoint, glm::vec3& outHitNormal, uint32_t& outBodyId);
        
        // Gravity
        void SetGravity(const glm::vec3& gravity);
        glm::vec3 GetGravity() const;

    private:
        Core::GameContext& m_Context;
        
        // Jolt systems
        std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
        std::unique_ptr<JPH::JobSystemSingleThreaded> m_JobSystem;
        std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;
        
        // Layer interfaces
        std::unique_ptr<JPH::BroadPhaseLayerInterface> m_BroadPhaseLayerInterface;
        std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_ObjectVsBroadPhaseLayerFilter;
        std::unique_ptr<JPH::ObjectLayerPairFilter> m_ObjectLayerPairFilter;
        
        // Listeners
        std::unique_ptr<JPH::BodyActivationListener> m_BodyActivationListener;
        std::unique_ptr<JPH::ContactListener> m_ContactListener;
        
        // Body ID to Entity mapping
        std::unordered_map<uint32_t, flecs::entity> m_BodyToEntity;
        
        // Characters
        std::vector<JPH::CharacterVirtual*> m_Characters;
        
        // Settings
        glm::vec3 m_Gravity = {0.0f, -9.81f, 0.0f};
        int m_CollisionSteps = 1;
        
        bool m_Initialized = false;
    };

}
