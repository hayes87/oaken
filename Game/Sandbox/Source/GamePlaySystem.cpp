#include "GamePlaySystem.h"
#include "GameComponents.h"
#include "Components/Components.h"
#include "Platform/Input.h"
#include "Core/EventBus.h"
#include "Core/Log.h"
#include <flecs.h>
#include <cmath>
#include <SDL3/SDL.h>

using namespace Platform;

GamePlaySystem::GamePlaySystem(Core::GameContext& context) 
    : m_Context(context) {}

void GamePlaySystem::Init() {
    if (m_Context.Events) {
        m_Context.Events->Subscribe<ActionEvent>([this](const ActionEvent& event) {
            this->OnAction(event);
        });
    }

    // Bounce System
    m_Context.World->system<LocalTransform, SpriteComponent>()
        .each([](flecs::entity e, LocalTransform& t, SpriteComponent& s) {
            float time = SDL_GetTicks() / 1000.0f;
            t.position.y = std::sin(time * 2.0f) * 0.5f;
        });

    // Orbit System - moves entities with OrbitComponent in circular patterns
    m_Context.World->system<LocalTransform, OrbitComponent>()
        .each([](flecs::entity e, LocalTransform& t, OrbitComponent& orbit) {
            float time = SDL_GetTicks() / 1000.0f;
            float angle = time * orbit.speed + orbit.phase;
            t.position.x = orbit.center.x + std::cos(angle) * orbit.radius;
            t.position.z = orbit.center.y + std::sin(angle) * orbit.radius;
        });
}

void GamePlaySystem::OnAction(const Platform::ActionEvent& event) {
    if (event.Action == "Cast_Slot_1"_hs) {
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
