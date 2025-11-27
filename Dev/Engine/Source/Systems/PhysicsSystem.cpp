#include "PhysicsSystem.h"
#include "../Components/Components.h"
#include "../Core/Log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

// Jolt includes - must be in specific order
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <cstdarg>
#include <thread>

// Jolt uses its own namespace
using namespace JPH;

// Callback for traces (debug logging)
static void TraceImpl(const char* fmt, ...) {
    va_list list;
    va_start(list, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);
    LOG_CORE_INFO("[Jolt] {}", buffer);
}

// Callback for asserts
#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* expression, const char* message, const char* file, uint line) {
    LOG_CORE_ERROR("[Jolt Assert] {}:{}: ({}) {}", file, line, expression, message ? message : "");
    return true; // Trigger breakpoint
}
#endif

// BroadPhaseLayerInterface implementation
class BroadPhaseLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        m_ObjectToBroadPhase[Systems::Layers::NON_MOVING] = BroadPhaseLayer(Systems::BroadPhaseLayers::NON_MOVING);
        m_ObjectToBroadPhase[Systems::Layers::MOVING] = BroadPhaseLayer(Systems::BroadPhaseLayers::MOVING);
        m_ObjectToBroadPhase[Systems::Layers::CHARACTER] = BroadPhaseLayer(Systems::BroadPhaseLayers::MOVING);
    }

    virtual uint GetNumBroadPhaseLayers() const override { return Systems::BroadPhaseLayers::NUM_LAYERS; }

    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        JPH_ASSERT(layer < Systems::Layers::NUM_LAYERS);
        return m_ObjectToBroadPhase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
        switch ((BroadPhaseLayer::Type)layer) {
            case Systems::BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case Systems::BroadPhaseLayers::MOVING: return "MOVING";
            default: return "INVALID";
        }
    }
#endif

private:
    BroadPhaseLayer m_ObjectToBroadPhase[Systems::Layers::NUM_LAYERS];
};

// ObjectVsBroadPhaseLayerFilter implementation
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(ObjectLayer layer1, BroadPhaseLayer layer2) const override {
        switch (layer1) {
            case Systems::Layers::NON_MOVING:
                return layer2 == BroadPhaseLayer(Systems::BroadPhaseLayers::MOVING);
            case Systems::Layers::MOVING:
            case Systems::Layers::CHARACTER:
                return true;
            default:
                return false;
        }
    }
};

// ObjectLayerPairFilter implementation  
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(ObjectLayer layer1, ObjectLayer layer2) const override {
        switch (layer1) {
            case Systems::Layers::NON_MOVING:
                return layer2 == Systems::Layers::MOVING || layer2 == Systems::Layers::CHARACTER;
            case Systems::Layers::MOVING:
            case Systems::Layers::CHARACTER:
                return true;
            default:
                return false;
        }
    }
};

// Body activation listener
class BodyActivationListenerImpl : public BodyActivationListener {
public:
    virtual void OnBodyActivated(const BodyID& bodyId, uint64 userData) override {
        (void)bodyId; (void)userData;
    }

    virtual void OnBodyDeactivated(const BodyID& bodyId, uint64 userData) override {
        (void)bodyId; (void)userData;
    }
};

// Contact listener for collision events
class ContactListenerImpl : public ContactListener {
public:
    virtual ValidateResult OnContactValidate(const Body& body1, const Body& body2, 
        RVec3Arg baseOffset, const CollideShapeResult& result) override {
        (void)body1; (void)body2; (void)baseOffset; (void)result;
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(const Body& body1, const Body& body2, 
        const ContactManifold& manifold, ContactSettings& settings) override {
        (void)body1; (void)body2; (void)manifold; (void)settings;
    }

    virtual void OnContactPersisted(const Body& body1, const Body& body2,
        const ContactManifold& manifold, ContactSettings& settings) override {
        (void)body1; (void)body2; (void)manifold; (void)settings;
    }

    virtual void OnContactRemoved(const SubShapeIDPair& pair) override {
        (void)pair;
    }
};

namespace Systems {

    PhysicsSystem::PhysicsSystem(Core::GameContext& context)
        : m_Context(context) {}

    PhysicsSystem::~PhysicsSystem() {
        Shutdown();
    }

    void PhysicsSystem::Init() {
        if (m_Initialized) return;

        LOG_CORE_INFO("Initializing Jolt Physics...");

        // Register allocation hook
        RegisterDefaultAllocator();

        // Install trace and assert callbacks
        Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
        AssertFailed = AssertFailedImpl;
#endif

        // Create factory
        Factory::sInstance = new Factory();

        // Register all Jolt physics types
        RegisterTypes();

        // Allocator for temporary allocations during physics update (10MB)
        m_TempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);

        // Job system with threads
        // Job system - use single-threaded for now (thread pool has CRT linkage issues)
        m_JobSystem = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);
        LOG_CORE_INFO("Jolt using single-threaded job system");

        // Create layer interfaces
        m_BroadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
        m_ObjectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
        m_ObjectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

        // Create physics system
        const uint cMaxBodies = 65536;
        const uint cNumBodyMutexes = 0; // Default
        const uint cMaxBodyPairs = 65536;
        const uint cMaxContactConstraints = 10240;

        m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
        m_PhysicsSystem->Init(
            cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            *m_BroadPhaseLayerInterface, *m_ObjectVsBroadPhaseLayerFilter, *m_ObjectLayerPairFilter
        );

        // Set gravity
        m_PhysicsSystem->SetGravity(Vec3(m_Gravity.x, m_Gravity.y, m_Gravity.z));

        // Create listeners
        m_BodyActivationListener = std::make_unique<BodyActivationListenerImpl>();
        m_ContactListener = std::make_unique<ContactListenerImpl>();
        m_PhysicsSystem->SetBodyActivationListener(m_BodyActivationListener.get());
        m_PhysicsSystem->SetContactListener(m_ContactListener.get());

        m_Initialized = true;
        LOG_CORE_INFO("Jolt Physics initialized successfully");
    }

    void PhysicsSystem::Shutdown() {
        if (!m_Initialized) return;

        LOG_CORE_INFO("Shutting down Jolt Physics...");

        // Destroy all characters
        for (auto* character : m_Characters) {
            delete character;
        }
        m_Characters.clear();

        // Clear body mapping
        m_BodyToEntity.clear();

        // Destroy physics system
        m_PhysicsSystem.reset();
        m_ContactListener.reset();
        m_BodyActivationListener.reset();
        m_ObjectLayerPairFilter.reset();
        m_ObjectVsBroadPhaseLayerFilter.reset();
        m_BroadPhaseLayerInterface.reset();
        m_JobSystem.reset();
        m_TempAllocator.reset();

        // Unregister types and destroy factory
        UnregisterTypes();
        delete Factory::sInstance;
        Factory::sInstance = nullptr;

        m_Initialized = false;
        LOG_CORE_INFO("Jolt Physics shutdown complete");
    }

    void PhysicsSystem::Step(float dt) {
        if (!m_Initialized || dt <= 0.0f) return;

        // Sync new/changed bodies to physics
        SyncBodiesToPhysics();

        // Update character physics first (they drive their own movement)
        UpdateCharacterPhysics(dt);

        // Step the physics simulation
        m_PhysicsSystem->Update(dt, m_CollisionSteps, m_TempAllocator.get(), m_JobSystem.get());

        // Sync physics results back to ECS transforms
        SyncPhysicsToTransforms();
    }

    void PhysicsSystem::SyncBodiesToPhysics() {
        BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

        // Query all entities with Collider + RigidBody that don't have a body yet
        m_Context.World->query<LocalTransform, Collider, RigidBody>()
            .each([&](flecs::entity entity, LocalTransform& transform, Collider& collider, RigidBody& rb) {
            if (rb.bodyId == 0xFFFFFFFF) {
                // Create a new body
                rb.bodyId = CreateBody(entity);
            }
        });
    }

    void PhysicsSystem::SyncPhysicsToTransforms() {
        BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

        // Update transforms for dynamic bodies
        m_Context.World->query<LocalTransform, const RigidBody>()
            .each([&](flecs::entity entity, LocalTransform& transform, const RigidBody& rb) {
            if (rb.bodyId == 0xFFFFFFFF) return;
            if (rb.motionType == MotionType::Static) return; // Static bodies don't move

            BodyID bodyId(rb.bodyId);
            if (!bodyInterface.IsActive(bodyId)) return;

            // Get position and rotation from Jolt
            RVec3 position = bodyInterface.GetPosition(bodyId);
            Quat rotation = bodyInterface.GetRotation(bodyId);

            // Update ECS transform
            transform.position = glm::vec3(position.GetX(), position.GetY(), position.GetZ());

            // Convert quaternion to Euler angles
            // Note: This is a simplified conversion, may have gimbal lock issues
            glm::quat q(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
            glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
            transform.rotation = euler;
        });
    }

    void PhysicsSystem::UpdateCharacterPhysics(float dt) {
        BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

        m_Context.World->query<LocalTransform, CharacterController, CharacterPhysics>()
            .each([&](flecs::entity entity, LocalTransform& transform, 
                                   CharacterController& controller, CharacterPhysics& physics) {
            if (physics.characterVirtual == nullptr) {
                // Create character
                physics.characterVirtual = CreateCharacter(entity);
            }

            CharacterVirtual* character = static_cast<CharacterVirtual*>(physics.characterVirtual);
            if (!character) return;

            // Set position from ECS (in case it was moved externally)
            character->SetPosition(RVec3(transform.position.x, transform.position.y, transform.position.z));

            // Apply velocity from CharacterController
            Vec3 velocity(controller.velocity.x, controller.velocity.y, controller.velocity.z);
            
            // Add gravity if not grounded
            if (!physics.isOnGround) {
                velocity += Vec3(m_Gravity.x, m_Gravity.y, m_Gravity.z) * dt;
            }

            character->SetLinearVelocity(velocity);

            // Update character
            CharacterVirtual::ExtendedUpdateSettings updateSettings;
            character->ExtendedUpdate(dt, Vec3(m_Gravity.x, m_Gravity.y, m_Gravity.z),
                updateSettings, m_PhysicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::CHARACTER),
                m_PhysicsSystem->GetDefaultLayerFilter(Layers::CHARACTER),
                {}, {}, *m_TempAllocator.get());

            // Update physics state
            physics.isOnGround = character->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
            Vec3 groundNormal = character->GetGroundNormal();
            physics.groundNormal = glm::vec3(groundNormal.GetX(), groundNormal.GetY(), groundNormal.GetZ());

            // Update ECS transform from character position
            RVec3 newPos = character->GetPosition();
            
            // // Debug: Log position updates periodically
            // static int frameCounter = 0;
            // if (frameCounter++ % 60 == 0) {
            //     LOG_CORE_INFO("Character physics: pos=({:.2f},{:.2f},{:.2f}), grounded={}, vel=({:.2f},{:.2f},{:.2f})",
            //         newPos.GetX(), newPos.GetY(), newPos.GetZ(),
            //         physics.isOnGround,
            //         velocity.GetX(), velocity.GetY(), velocity.GetZ());
            // }
            
            transform.position = glm::vec3(newPos.GetX(), newPos.GetY(), newPos.GetZ());
            transform.position = glm::vec3(newPos.GetX(), newPos.GetY(), newPos.GetZ());

            // Update controller grounded state
            controller.isGrounded = physics.isOnGround;
        });
    }

    uint32_t PhysicsSystem::CreateBody(flecs::entity entity) {
        if (!m_Initialized) return 0xFFFFFFFF;

        if (!entity.has<LocalTransform>() || !entity.has<Collider>() || !entity.has<RigidBody>()) {
            return 0xFFFFFFFF;
        }

        const LocalTransform& transform = entity.get<LocalTransform>();
        const Collider& collider = entity.get<Collider>();
        const RigidBody& rb = entity.get<RigidBody>();

        // Create shape based on collider type
        ShapeSettings::ShapeResult shapeResult;
        switch (collider.type) {
            case ColliderType::Box:
                shapeResult = BoxShapeSettings(Vec3(collider.size.x, collider.size.y, collider.size.z)).Create();
                break;
            case ColliderType::Sphere:
                shapeResult = SphereShapeSettings(collider.size.x).Create();
                break;
            case ColliderType::Capsule:
                shapeResult = CapsuleShapeSettings(collider.size.y * 0.5f, collider.size.x).Create();
                break;
            default:
                LOG_CORE_ERROR("Unsupported collider type");
                return 0xFFFFFFFF;
        }

        if (!shapeResult.IsValid()) {
            LOG_CORE_ERROR("Failed to create collision shape");
            return 0xFFFFFFFF;
        }

        // Determine Jolt motion type
        EMotionType joltMotionType;
        ObjectLayer layer;
        switch (rb.motionType) {
            case MotionType::Static:
                joltMotionType = EMotionType::Static;
                layer = Layers::NON_MOVING;
                break;
            case MotionType::Kinematic:
                joltMotionType = EMotionType::Kinematic;
                layer = Layers::MOVING;
                break;
            case MotionType::Dynamic:
            default:
                joltMotionType = EMotionType::Dynamic;
                layer = Layers::MOVING;
                break;
        }

        // Create body
        RVec3 position(transform.position.x + collider.offset.x,
                       transform.position.y + collider.offset.y,
                       transform.position.z + collider.offset.z);

        // Convert Euler angles to quaternion
        glm::quat q = glm::quat(glm::radians(transform.rotation));
        Quat rotation(q.x, q.y, q.z, q.w);

        BodyCreationSettings bodySettings(shapeResult.Get(), position, rotation, joltMotionType, layer);
        bodySettings.mFriction = rb.friction;
        bodySettings.mRestitution = rb.restitution;
        bodySettings.mLinearDamping = rb.linearDamping;
        bodySettings.mAngularDamping = rb.angularDamping;

        if (rb.motionType == MotionType::Dynamic) {
            bodySettings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            bodySettings.mMassPropertiesOverride.mMass = rb.mass;
        }

        BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, 
            rb.motionType == MotionType::Static ? EActivation::DontActivate : EActivation::Activate);

        if (bodyId.IsInvalid()) {
            LOG_CORE_ERROR("Failed to create physics body");
            return 0xFFFFFFFF;
        }

        m_BodyToEntity[bodyId.GetIndexAndSequenceNumber()] = entity;
        LOG_CORE_INFO("Created physics body {} for entity", bodyId.GetIndexAndSequenceNumber());

        return bodyId.GetIndexAndSequenceNumber();
    }

    void PhysicsSystem::DestroyBody(uint32_t bodyId) {
        if (!m_Initialized || bodyId == 0xFFFFFFFF) return;

        BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
        BodyID id(bodyId);
        
        bodyInterface.RemoveBody(id);
        bodyInterface.DestroyBody(id);
        m_BodyToEntity.erase(bodyId);
    }

    void* PhysicsSystem::CreateCharacter(flecs::entity entity) {
        if (!m_Initialized) return nullptr;

        if (!entity.has<LocalTransform>() || !entity.has<CharacterPhysics>()) {
            return nullptr;
        }

        const LocalTransform& transform = entity.get<LocalTransform>();
        const CharacterPhysics& physics = entity.get<CharacterPhysics>();

        // Create capsule shape for character
        RefConst<Shape> standingShape = new CapsuleShape(physics.height * 0.5f, physics.radius);

        CharacterVirtualSettings settings;
        settings.mShape = standingShape;
        settings.mMass = physics.mass;
        settings.mMaxSlopeAngle = DegreesToRadians(physics.maxSlopeAngle);
        settings.mMaxStrength = 100.0f;
        settings.mBackFaceMode = EBackFaceMode::CollideWithBackFaces;
        settings.mCharacterPadding = 0.02f;
        settings.mPenetrationRecoverySpeed = 1.0f;
        settings.mPredictiveContactDistance = 0.1f;

        RVec3 position(transform.position.x, transform.position.y, transform.position.z);
        Quat rotation = Quat::sIdentity();

        CharacterVirtual* character = new CharacterVirtual(&settings, position, rotation, m_PhysicsSystem.get());
        m_Characters.push_back(character);

        LOG_CORE_INFO("Created character physics for entity");
        return character;
    }

    void PhysicsSystem::DestroyCharacter(void* character) {
        if (!character) return;

        CharacterVirtual* charPtr = static_cast<CharacterVirtual*>(character);
        auto it = std::find(m_Characters.begin(), m_Characters.end(), charPtr);
        if (it != m_Characters.end()) {
            m_Characters.erase(it);
            delete charPtr;
        }
    }

    bool PhysicsSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                glm::vec3& outHitPoint, glm::vec3& outHitNormal, uint32_t& outBodyId) {
        if (!m_Initialized) return false;

        RRayCast ray(RVec3(origin.x, origin.y, origin.z), 
                     Vec3(direction.x, direction.y, direction.z) * maxDistance);

        RayCastResult result;
        bool hit = m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(ray, result);

        if (hit) {
            RVec3 hitPoint = ray.GetPointOnRay(result.mFraction);
            outHitPoint = glm::vec3(hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ());

            // Get body interface to query normal
            BodyLockRead lock(m_PhysicsSystem->GetBodyLockInterface(), result.mBodyID);
            if (lock.Succeeded()) {
                Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, hitPoint);
                outHitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
            }

            outBodyId = result.mBodyID.GetIndexAndSequenceNumber();
            return true;
        }

        return false;
    }

    void PhysicsSystem::SetGravity(const glm::vec3& gravity) {
        m_Gravity = gravity;
        if (m_Initialized) {
            m_PhysicsSystem->SetGravity(Vec3(gravity.x, gravity.y, gravity.z));
        }
    }

    glm::vec3 PhysicsSystem::GetGravity() const {
        return m_Gravity;
    }

}
