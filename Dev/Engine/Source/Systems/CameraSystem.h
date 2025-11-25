#pragma once
#include <flecs.h>
#include "../Platform/Input.h"

namespace Systems {
    class CameraSystem {
    public:
        CameraSystem(flecs::world& world, Platform::Input& input);
    };
}
