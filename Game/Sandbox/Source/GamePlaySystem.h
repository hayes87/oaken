#pragma once

#include "Core/Context.h"

namespace Platform { struct ActionEvent; }

class GamePlaySystem {
public:
    GamePlaySystem(Core::GameContext& context);
    void Init();

private:
    Core::GameContext& m_Context;
    void OnAction(const Platform::ActionEvent& event);
};
