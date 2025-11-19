#include "Engine.h"
#include <string>

int main(int argc, char* argv[]) {
    Engine engine;
    
    if (engine.Init()) {
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
