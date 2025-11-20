#include "Engine.h"
#include "GamePlaySystem.h"
#include "GameComponents.h"
#include "Core/Log.h"
#include "Components/Components.h"
#include "Resources/Texture.h"
#include <memory>
#include <filesystem>

// Global state to keep alive between reloads if needed, 
// but for now we just re-init.
std::unique_ptr<GamePlaySystem> g_GamePlaySystem;
std::shared_ptr<Resources::Texture> g_TestTexture;

GAME_EXPORT void GameInit(Engine& engine) {
    LOG_INFO("GameInit: Initializing Sandbox Game Module");

    // Register Game Components
    engine.GetContext().World->component<AttributeSet>();

    // Load Test Texture
    // Path is relative to the executable (Build/Game/Sandbox/Debug/Sandbox.exe)
    // Assets are in Build/Game/Sandbox/Cooked/Assets/
    std::string assetPath = "../Cooked/Assets/test.oaktex";
    if (std::filesystem::exists(assetPath)) {
        g_TestTexture = engine.GetResourceManager().LoadTexture(assetPath);
        if (g_TestTexture) {
            LOG_INFO("Successfully loaded test texture: {}", assetPath);
            LOG_INFO("Texture Size: {}x{}", g_TestTexture->GetWidth(), g_TestTexture->GetHeight());
        } else {
            LOG_ERROR("Failed to load texture: {}", assetPath);
        }
    } else {
        LOG_WARN("Asset not found at: {}", assetPath);
        // Try absolute path for debugging if needed, or just warn.
    }

    // Initialize Game Systems
    g_GamePlaySystem = std::make_unique<GamePlaySystem>(engine.GetContext());
    g_GamePlaySystem->Init();

    // Create a test entity with AttributeSet if it doesn't exist
    // For hot reload, we might want to avoid creating duplicates.
    // Simple check:
    if (engine.GetContext().World->count<AttributeSet>() == 0) {
        auto e = engine.GetContext().World->entity("Player")
            .set<AttributeSet>({100.0f, 100.0f, 100.0f, 100.0f, 10.0f});
            
        if (g_TestTexture) {
            e.set<SpriteComponent>({g_TestTexture, {1,1,1,1}});
            LOG_INFO("Added SpriteComponent to Player entity");
        }
    } else {
        // If entity exists (reloaded), update texture
        auto e = engine.GetContext().World->lookup("Player");
        if (e && g_TestTexture) {
            e.set<SpriteComponent>({g_TestTexture, {1,1,1,1}});
            LOG_INFO("Updated SpriteComponent on Player entity");
        }
    }
}

GAME_EXPORT void GameShutdown(Engine& engine) {
    LOG_INFO("GameShutdown: Unloading Sandbox Game Module");
    g_GamePlaySystem.reset();
    // We don't unregister components usually as that clears data
}
