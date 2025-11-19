# Oaken Engine Walkthrough

## Overview
The Oaken Engine has been initialized with the following structure:
- **Core Layer**: HashedString, EventBus, Context, TimeStep.
- **Platform Layer**: SDL3 Window, Input, and RenderDevice (SDL_GPU).
- **Engine Layer**: Main loop with fixed time step and Flecs integration.

## Prerequisites
- **CMake**: Version 3.25 or higher.
- **Vcpkg**: For dependency management.
- **C++ Compiler**: Supporting C++20 (MSVC, Clang, or GCC).

## Build Instructions

1.  **Configure with CMake**:
    ```bash
    cmake -S . -B Build -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake
    ```
    *Note: Replace `[path/to/vcpkg]` with the actual path to your vcpkg installation.*

2.  **Build**:
    ```bash
    cmake --build Build --config Debug
    ```

3.  **Run**:
    ```bash
    ./Build/Debug/OakenEngine.exe
    ```

## Next Steps
- Implement **AssetManager** in `Source/Resources`.
- Add **Systems** in `Source/Systems` (RenderSystem, PhysicsSystem, etc.).
- Integrate **Jolt Physics** and **ImGui**.

## Directory Structure
- `Source/Core`: Fundamental types.
- `Source/Platform`: SDL3 wrappers.
- `Source/Engine.h/cpp`: Main entry point and loop.
