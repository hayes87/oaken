#pragma once

#include <SDL3/SDL.h>
#include <unordered_map>
#include "../Core/HashedString.h"

namespace Core { class EventBus; }

namespace Platform {

    struct ActionEvent {
        Core::HashedString Action;
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

    private:
        Core::EventBus* m_EventBus = nullptr;
        SDL_Window* m_Window = nullptr;
        const bool* m_KeyboardState = nullptr;
        int m_NumKeys = 0;
        
        std::unordered_map<uint32_t, SDL_Scancode> m_ActionMap; // Hash -> Scancode
        float m_MouseDeltaX = 0.0f;
        float m_MouseDeltaY = 0.0f;
        float m_ScrollDelta = 0.0f;
    };

}
