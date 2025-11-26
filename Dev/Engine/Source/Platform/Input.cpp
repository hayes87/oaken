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
        
        // Reset scroll delta (it's accumulated in ProcessEvent)
        m_ScrollDelta = 0.0f;
        
        // Update generic input axes from keyboard/mouse/gamepad
        UpdateAxes();
    }
    
    void Input::UpdateAxes() {
        // Reset axes
        m_Axes = InputAxes{};
        
        // Movement from keyboard (WASD)
        if (IsKeyDown(SDL_SCANCODE_W)) m_Axes.move.y += 1.0f;
        if (IsKeyDown(SDL_SCANCODE_S)) m_Axes.move.y -= 1.0f;
        if (IsKeyDown(SDL_SCANCODE_A)) m_Axes.move.x -= 1.0f;
        if (IsKeyDown(SDL_SCANCODE_D)) m_Axes.move.x += 1.0f;
        
        // Normalize movement if using keyboard (digital input)
        if (glm::length(m_Axes.move) > 1.0f) {
            m_Axes.move = glm::normalize(m_Axes.move);
        }
        
        // Look from mouse delta
        m_Axes.look.x = m_MouseDeltaX;
        m_Axes.look.y = m_MouseDeltaY;
        
        // Zoom from scroll wheel
        m_Axes.zoom = m_ScrollDelta;
        
        // Sprint from Left Shift
        m_Axes.sprint = IsKeyDown(SDL_SCANCODE_LSHIFT);
        
        // Jump from Space
        m_Axes.jump = IsKeyDown(SDL_SCANCODE_SPACE);
        
        // TODO: Add gamepad support here
        // SDL_Gamepad* gamepad = SDL_GetGamepad(...);
        // if (gamepad) {
        //     // Left stick for movement
        //     float lx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        //     float ly = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        //     if (glm::length(glm::vec2(lx, ly)) > 0.1f) { // Dead zone
        //         m_Axes.move = glm::vec2(lx, -ly);
        //     }
        //     
        //     // Right stick for camera
        //     float rx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        //     float ry = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
        //     m_Axes.look += glm::vec2(rx, ry) * 10.0f; // Scale for gamepad
        //     
        //     // Triggers for zoom or sprint
        //     m_Axes.sprint |= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        //     m_Axes.jump |= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
        // }
    }
    
    // New function to handle individual SDL events
    void Input::ProcessEvent(const SDL_Event& event) {
        // Handle scroll wheel
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            m_ScrollDelta += event.wheel.y;
        }
        
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
