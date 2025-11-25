#include "Input.h"
#include "../Core/EventBus.h"

namespace Platform {

    void Input::Init(Core::EventBus* eventBus, SDL_Window* window) {
        m_EventBus = eventBus;
        m_Window = window;
        // SDL_GetKeyboardState returns a pointer to the internal state array
        // It is valid for the whole lifetime of the application
        m_KeyboardState = SDL_GetKeyboardState(&m_NumKeys);
    }

    void Input::Poll() {
        SDL_PumpEvents();
        
        // Update mouse delta once per frame
        SDL_GetRelativeMouseState(&m_MouseDeltaX, &m_MouseDeltaY);
        
        // Simple polling for actions to dispatch events
        // In a real scenario, we might want to use SDL events directly for "pressed this frame" logic
        // or keep track of previous state to detect edges.
        
        // For now, let's just check our mapped actions and fire events if they are down
        // NOTE: This will fire every frame the key is held. 
        // Usually we want "JustPressed".
        
        // Let's implement a simple "JustPressed" check using previous state if we had it.
        // But for now, let's iterate the action map and check keys.
        
        // To do "JustPressed" properly without keeping a full copy of keyboard state:
        // We can use the SDL event loop in Engine.cpp to feed us events, OR
        // we can keep a small set of "Active Actions" from last frame.
        
        // For this milestone, let's stick to the user's request:
        // "On key press, publish ActionEvent { "Jump"_hs }."
        
        // We'll rely on the SDL event loop in Engine.cpp calling a function here, 
        // OR we can peek at events here.
        // The user said: "Listen to SDL events."
        
        // Let's add a ProcessEvent function that Engine.cpp calls.
    }
    
    // New function to handle individual SDL events
    void Input::ProcessEvent(const SDL_Event& event) {
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            // Find if this key is mapped to an action
            for (const auto& [hash, scancode] : m_ActionMap) {
                if (scancode == event.key.scancode) {
                    if (m_EventBus) {
                        m_EventBus->Publish(ActionEvent{ Core::HashedString(hash) });
                    }
                }
            }
        }
    }

    bool Input::IsActionActive(Core::HashedString action) const {
        auto it = m_ActionMap.find(action.GetHash());
        if (it != m_ActionMap.end()) {
            return IsKeyDown(it->second);
        }
        return false;
    }

    bool Input::IsKeyDown(SDL_Scancode key) const {
        if (m_KeyboardState && key < m_NumKeys) {
            return m_KeyboardState[key];
        }
        return false;
    }

    void Input::MapAction(Core::HashedString action, SDL_Scancode key) {
        m_ActionMap[action.GetHash()] = key;
    }

    void Input::SetRelativeMouseMode(bool enabled) {
        if (m_Window) {
            SDL_SetWindowRelativeMouseMode(m_Window, enabled);
        }
    }

    void Input::GetMouseDelta(float& x, float& y) {
        x = m_MouseDeltaX;
        y = m_MouseDeltaY;
    }

}
