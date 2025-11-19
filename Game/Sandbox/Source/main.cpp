#include "Engine.h"
#include "GamePlaySystem.h"
#include "GameComponents.h"
#include "Core/Log.h"
#include <memory>

// Global state to keep alive between reloads if needed, 
// but for now we just re-init.
std::unique_ptr<GamePlaySystem> g_GamePlaySystem;

GAME_EXPORT void GameInit(Engine& engine) {
    LOG_INFO("GameInit: Initializing Sandbox Game Module");

    // Register Game Components
    // Note: In a real hot-reload scenario, we should check if already registered
    // or Flecs handles it gracefully (it usually does).
    engine.GetContext().World->component<AttributeSet>();

    // Initialize Game Systems
    g_GamePlaySystem = std::make_unique<GamePlaySystem>(engine.GetContext());
    g_GamePlaySystem->Init();

    // Create a test entity with AttributeSet if it doesn't exist
    // For hot reload, we might want to avoid creating duplicates.
    // Simple check:
    if (engine.GetContext().World->count<AttributeSet>() == 0) {
        engine.GetContext().World->entity("Player")
            .set<AttributeSet>({100.0f, 100.0f, 100.0f, 100.0f, 10.0f});
    }
}

GAME_EXPORT void GameShutdown(Engine& engine) {
    LOG_INFO("GameShutdown: Unloading Sandbox Game Module");
    g_GamePlaySystem.reset();
    // We don't unregister components usually as that clears data
}
