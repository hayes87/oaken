#pragma once

#include "../Core/Context.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace Systems {

    class PhysicsSystem {
    public:
        PhysicsSystem(Core::GameContext& context);
        ~PhysicsSystem();
        
        void Init();
        void Step(double dt);

    private:
        Core::GameContext& m_Context;
        // Jolt Physics members would go here
        // JPH::PhysicsSystem m_PhysicsSystem;
    };

}
