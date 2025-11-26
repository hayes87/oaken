#pragma once

#include <SDL3/SDL.h>
#include <unordered_map>
#include <glm/glm.hpp>
#include "../Core/HashedString.h"

namespace Core { class EventBus; }

namespace Platform {

    struct ActionEvent {
        Core::HashedString Action;
    };

    // Generic input axes that can be mapped to keyboard, mouse, or gamepad
    struct InputAxes {
        glm::vec2 move = {0.0f, 0.0f};      // Movement input (WASD or left stick)
        glm::vec2 look = {0.0f, 0.0f};      // Camera look (mouse delta or right stick)
        float zoom = 0.0f;                   // Zoom (scroll wheel or triggers)
        bool sprint = false;                 // Sprint modifier
        bool jump = false;                   // Jump action
    };

    class Input {
    public:
        void Init(Core::EventBus* eventBus, SDL_Window* window);
        void Poll();
        void ProcessEvent(const SDL_Event& event);

        bool IsActionActive(Core::HashedString action) const;
        bool IsKeyDown(SDL_Scancode key) const;
        void MapAction(Core::HashedString action, SDL_Scancode key);

        // Mouse Support
        void SetRelativeMouseMode(bool enabled);
        void GetMouseDelta(float& x, float& y);
        float GetScrollDelta() const { return m_ScrollDelta; }

        // Generic input axes (combines keyboard/mouse/gamepad)
        const InputAxes& GetAxes() const { return m_Axes; }
        glm::vec2 GetMoveInput() const { return m_Axes.move; }
        glm::vec2 GetLookInput() const { return m_Axes.look; }
        float GetZoomInput() const { return m_Axes.zoom; }
        bool IsSprinting() const { return m_Axes.sprint; }
        bool IsJumping() const { return m_Axes.jump; }

    private:
        void UpdateAxes();

        Core::EventBus* m_EventBus = nullptr;
        SDL_Window* m_Window = nullptr;
        const bool* m_KeyboardState = nullptr;
        int m_NumKeys = 0;
        
        std::unordered_map<uint32_t, SDL_Scancode> m_ActionMap; // Hash -> Scancode
        float m_MouseDeltaX = 0.0f;
        float m_MouseDeltaY = 0.0f;
        float m_ScrollDelta = 0.0f;

        InputAxes m_Axes;
    };

}
