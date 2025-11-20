#pragma once
#include "../Core/Context.h"

namespace Systems {
    class TransformSystem {
    public:
        TransformSystem(Core::GameContext& context);
        void Init();

    private:
        Core::GameContext& m_Context;
    };
}
