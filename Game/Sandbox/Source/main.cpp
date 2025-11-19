#include "Engine.h"
#include "GamePlaySystem.h"
#include "GameComponents.h"
#include <string>
#include <memory>

int main(int argc, char* argv[]) {
    Engine engine;
    
    if (engine.Init()) {
        // Initialize Game Systems
        auto gamePlaySystem = std::make_unique<GamePlaySystem>(engine.GetContext());
        gamePlaySystem->Init();

        // Register Game Components
        engine.GetContext().World->component<AttributeSet>();

        // Create a test entity with AttributeSet
        engine.GetContext().World->entity("Player")
            .set<AttributeSet>({100.0f, 100.0f, 100.0f, 100.0f, 10.0f});

        // Simple argument parsing
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--time-limit" && i + 1 < argc) {
                double limit = std::stod(argv[i + 1]);
                engine.SetTimeLimit(limit);
            }
        }

        engine.Run();
    }
    
    return 0;
}
