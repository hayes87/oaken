#pragma once

namespace flecs {
    class world;
}

namespace Core {
    class EventBus;
    // Forward declare AssetManager when we have it
    // class AssetManager; 

    struct GameContext {
        flecs::world* World = nullptr;
        EventBus* Events = nullptr;
        // AssetManager* Assets = nullptr;
    };
}
