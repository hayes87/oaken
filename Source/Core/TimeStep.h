#pragma once

namespace Core {

    struct TimeStep {
        double DeltaTime = 0.0;
        double TotalTime = 0.0;
        
        // Fixed time step for physics
        static constexpr double FixedDeltaTime = 1.0 / 60.0;
    };

}
