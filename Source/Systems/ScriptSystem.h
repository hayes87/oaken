#pragma once

#include "../Core/Context.h"
#include <sol/sol.hpp>

namespace Systems {

    class ScriptSystem {
    public:
        ScriptSystem(Core::GameContext& context);
        
        void Init();
        void Update(double dt);

    private:
        Core::GameContext& m_Context;
        sol::state m_Lua;
    };

}
