#pragma once

#include "../Core/Context.h"

namespace Platform { struct ActionEvent; }

namespace Systems {

    class AbilitySystem {
    public:
        AbilitySystem(Core::GameContext& context);
        
        void Init();
        void TickCooldowns(double dt);

    private:
        Core::GameContext& m_Context;
        
        void OnAction(const Platform::ActionEvent& event);
    };

}
