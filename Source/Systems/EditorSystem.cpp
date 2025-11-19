#include "EditorSystem.h"
#include <imgui.h>
#include <iostream>

namespace Systems {

    EditorSystem::EditorSystem(Core::GameContext& context)
        : m_Context(context) {}

    void EditorSystem::Init() {
        // Init ImGui context
        // ImGui::CreateContext();
        // Init SDL3/SDL_GPU backends
    }

    void EditorSystem::DrawUI(flecs::world* world) {
        (void)world;
        // ImGui::NewFrame();
        
        // Draw Hierarchy
        // Draw Inspector
        
        // ImGui::Render();
        // DrawData...
    }

}
