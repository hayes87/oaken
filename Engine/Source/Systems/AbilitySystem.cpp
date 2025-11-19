#include "AbilitySystem.h"
#include "../Components/Components.h"
#include "../Platform/Input.h" 
#include "../Core/EventBus.h"
#include "../Core/Log.h"
#include <flecs.h>

#include <iostream>

using namespace Platform;

namespace Systems {

    AbilitySystem::AbilitySystem(Core::GameContext& context) 
        : m_Context(context) {}

    void AbilitySystem::Init() {
        if (m_Context.Events) {
            m_Context.Events->Subscribe<ActionEvent>([this](const ActionEvent& event) {
                this->OnAction(event);
            });
        }
    }

    void AbilitySystem::TickCooldowns(double dt) {
        (void)dt;
        // Iterate entities with cooldowns if we had that component
    }

    void AbilitySystem::OnAction(const Platform::ActionEvent& event) {
        // Generic ability triggering logic would go here
        (void)event;
    }

}
