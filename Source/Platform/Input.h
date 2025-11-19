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
        void Init(Core::EventBus* eventBus);
        void Poll();
        void ProcessEvent(const SDL_Event& event);
        
        bool IsActionActive(Core::HashedString action) const;
        bool IsKeyDown(SDL_Scancode key) const;

        // Map a key to an action
        void MapAction(Core::HashedString action, SDL_Scancode key);

    private:
        Core::EventBus* m_EventBus = nullptr;
        const bool* m_KeyboardState = nullptr;
        int m_NumKeys = 0;
        
        std::unordered_map<uint32_t, SDL_Scancode> m_ActionMap; // Hash -> Scancode
    };

}
