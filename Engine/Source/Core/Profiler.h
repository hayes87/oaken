#pragma once

#include <tracy/Tracy.hpp>

// Macros for easy usage
#define PROFILE_FRAME(name) FrameMark
#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_FUNCTION() ZoneScoped

// We can add GPU profiling macros here later when we have the RenderDevice set up
