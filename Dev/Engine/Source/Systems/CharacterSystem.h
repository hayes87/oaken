#pragma once
#include <flecs.h>
#include "../Platform/Input.h"

namespace Systems {
    class CharacterSystem {
    public:
        CharacterSystem(flecs::world& world, Platform::Input& input);
    };
}
