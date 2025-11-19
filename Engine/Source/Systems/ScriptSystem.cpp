#include "ScriptSystem.h"
#include "../Components/Components.h"
#include <flecs.h>
#include <iostream>

namespace Systems {

    ScriptSystem::ScriptSystem(Core::GameContext& context)
        : m_Context(context) {}

    void ScriptSystem::Init() {
        m_Lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math);
        
        // Bind Entity and Components
        // m_Lua.new_usertype<Transform>("Transform", ...);
    }

    void ScriptSystem::Update(double dt) {
        (void)dt;
        // Iterate entities with ScriptComponent
        m_Context.World->each([this](flecs::entity e, ScriptComponent& s) {
            (void)e;
            (void)s;
            // Run script
            // m_Lua.script_file(s.scriptName);
            // Call OnUpdate(dt)
        });
    }

}
