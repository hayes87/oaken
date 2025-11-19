#pragma once

#include "../Core/Context.h"
#include <flecs.h>

namespace Systems {

    class EditorSystem {
    public:
        EditorSystem(Core::GameContext& context);
        
        void Init();
        void DrawUI(flecs::world* world);

    private:
        Core::GameContext& m_Context;
    };

}
