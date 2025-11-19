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
        if (event.Action == "Cast_Slot_1"_hs) {
            // Query AttributeSet
            // We need to find the "Player" entity or the source of the action.
            // For now, let's just query ALL entities with AttributeSet (as a test)
            // or assume a single player.
            
            // In a real game, the event would carry the EntityID.
            
            m_Context.World->each([](flecs::entity e, AttributeSet& attrs) {
                (void)e;
                float cost = 10.0f;
                if (attrs.mana >= cost) {
                    attrs.mana -= cost;
                    LOG_INFO("Casting Spell! Mana: {}", attrs.mana);
                } else {
                    LOG_WARN("Not enough mana! Current: {}", attrs.mana);
                }
            });
        }
    }

}
