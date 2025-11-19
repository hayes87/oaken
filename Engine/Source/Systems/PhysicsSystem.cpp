#include "PhysicsSystem.h"
#include "../Components/Components.h"
#include <iostream>

namespace Systems {

    PhysicsSystem::PhysicsSystem(Core::GameContext& context)
        : m_Context(context) {}

    PhysicsSystem::~PhysicsSystem() {
        // Shutdown Jolt
    }

    void PhysicsSystem::Init() {
        // Initialize Jolt
        // Register allocation hook, factory, etc.
    }

    void PhysicsSystem::Step(double dt) {
        (void)dt;
        // 1. Pre-Step: Sync ECS Transform -> Jolt Body
        
        // 2. Step Simulation
        // m_PhysicsSystem.Update(dt, 1, 1, m_TempAllocator, m_JobSystem);
        
        // 3. Post-Step: Sync Jolt Body -> ECS Transform
    }

}
