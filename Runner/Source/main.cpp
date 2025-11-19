#include "Engine.h"
#include <string>
#include <iostream>
#include <filesystem>
#include <SDL3/SDL.h>

// Define function pointers for Game DLL
typedef void (*GameInitFunc)(Engine&);
typedef void (*GameShutdownFunc)(Engine&);

struct GameModule {
    SDL_SharedObject* Handle = nullptr;
    GameInitFunc Init = nullptr;
    GameShutdownFunc Shutdown = nullptr;
    std::filesystem::file_time_type LastWriteTime;
};

bool LoadGameModule(const std::string& path, GameModule& module, Engine& engine) {
    // Unload existing
    if (module.Handle) {
        if (module.Shutdown) {
            module.Shutdown(engine);
        }
        SDL_UnloadObject(module.Handle);
        module.Handle = nullptr;
    }

    // Copy DLL to a temp file to avoid locking the original
    std::string tempPath = path + ".temp";
    try {
        std::filesystem::copy_file(path, tempPath, std::filesystem::copy_options::overwrite_existing);
    } catch (std::exception& e) {
        std::cerr << "Failed to copy game DLL: " << e.what() << std::endl;
        return false;
    }

    module.Handle = SDL_LoadObject(tempPath.c_str());
    if (!module.Handle) {
        std::cerr << "Failed to load game DLL: " << SDL_GetError() << std::endl;
        return false;
    }

    module.Init = (GameInitFunc)SDL_LoadFunction(module.Handle, "GameInit");
    module.Shutdown = (GameShutdownFunc)SDL_LoadFunction(module.Handle, "GameShutdown");

    if (!module.Init || !module.Shutdown) {
        std::cerr << "Failed to load game functions" << std::endl;
        return false;
    }

    module.Init(engine);
    module.LastWriteTime = std::filesystem::last_write_time(path);
    
    std::cout << "Game Module Loaded Successfully!" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "Runner Starting..." << std::endl;

    // Parse args
    const std::string gameDllPath = "Game.dll";
    double timeLimit = 0.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--time-limit" && i + 1 < argc) {
            timeLimit = std::stod(argv[i + 1]);
            i++; // Skip next arg
        }
    }

    std::cout << "Loading Game Module: " << gameDllPath << std::endl;

    Engine engine;
    if (timeLimit > 0.0) {
        engine.SetTimeLimit(timeLimit);
    }
    
    if (!engine.Init()) {
        std::cerr << "Engine Init Failed" << std::endl;
        return -1;
    }

    GameModule gameModule;
    
    // Initial Load
    if (!LoadGameModule(gameDllPath, gameModule, engine)) {
        std::cerr << "Critical: Failed to load initial game module." << std::endl;
        // Continue anyway? Or exit?
    }

    // Main Loop
    while (engine.Step()) {
        // Check for hot reload
        try {
            auto currentWriteTime = std::filesystem::last_write_time(gameDllPath);
            if (currentWriteTime > gameModule.LastWriteTime) {
                std::cout << "Detected change in " << gameDllPath << std::endl;
                // Wait a bit for write to finish?
                SDL_Delay(100); 
                std::cout << "Reloading Game Module..." << std::endl;
                LoadGameModule(gameDllPath, gameModule, engine);
            }
        } catch (const std::exception& e) {
             std::cerr << "Hot reload check failed: " << e.what() << std::endl;
        } catch (...) {
            // File might be locked or busy
        }
    }

    // Shutdown
    if (gameModule.Handle) {
        if (gameModule.Shutdown) {
            gameModule.Shutdown(engine);
        }
        SDL_UnloadObject(gameModule.Handle);
    }

    engine.Shutdown();
    return 0;
}
