# Oaken Engine Copilot Instructions

## Project Architecture
- **Engine (`Dev/Engine`)**: Core shared library (DLL). Contains `Core`, `Platform` (SDL3), `Resources`, `Scene`, and `Systems`.
- **Runner (`Dev/Runner`)**: Generic executable that loads the Engine and a Game DLL. Handles the main loop and hot-reloading of the Game DLL.
- **AssetCooker (`Dev/AssetCooker`)**: Command-line tool to process raw assets (`.png`, `.obj`, `.vert`) into binary runtime formats (`.oaktex`, `.oakmesh`, `.spv`).
- **Game (`Game/Sandbox`)**: Game logic compiled as a DLL. Links against `OakenEngine`.

## Tech Stack
- **Language**: C++20.
- **Dependencies**: SDL3 (Window/Input/GPU), Flecs (ECS), GLM (Math), Jolt Physics, spdlog, fastgltf, Assimp.
- **Build System**: CMake + Vcpkg.

## Developer Workflows
- DO NOT CONSIDER A TASK DONE UNTIL YOU HAVE FULLY TESTED IT.

### Building
- Configure: `cmake -S . -B Build -DCMAKE_TOOLCHAIN_FILE=[vcpkg_path]/scripts/buildsystems/vcpkg.cmake`
- Build: `cmake --build Build --config Debug`

### Asset Pipeline (Crucial)
- **Never load raw assets directly in the Engine.**
- **Cooking**: Use `AssetCooker` to convert assets.
  - Textures: `COOK TEXTURE input.png output.oaktex`
  - Meshes: `COOK MESH input.obj output.oakmesh`
  - Scenes: `COOK SCENE input.oakscene output.oaklevel`
  - Shaders: `COOK SHADER input.vert output.vert.spv`
- **Runtime Loading**: `ResourceManager` loads cooked assets from `Assets/` (relative to executable).
  - Shaders: `Assets/Shaders/Mesh.vert.spv` (Vulkan) or `.dxil` (D3D12).

### Rendering (SDL_GPU)
- **Backend**: Uses `SDL_GPU` (Vulkan/D3D12 abstraction).
- **Shaders**: Must be compiled to SPIR-V (Vulkan) or DXIL (D3D12).
- **Pipeline**: `RenderSystem` manages `SDL_GPUGraphicsPipeline`.
- **Coordinate System**: GLM (Right-handed, Y-up).

### ECS (Flecs)
- **Components**: Plain structs in `Components.h`.
- **Systems**: Classes in `Systems/` or `flecs::system` registrations.
- **Entities**: Created in `Scene` or via `SceneSerializer`.

## Coding Conventions
- **Logging**: Use `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` (wraps spdlog).
- **Hot Reload**:
  - **Game Logic**: `Runner` watches `Game.dll`. `GameInit` and `GameShutdown` are the entry points.
  - **Resources**: `ResourceManager` watches file timestamps and calls `Reload()` on resources.
- **Serialization**:
  - `SceneSerializer`: JSON (Dev/Editor) <-> Binary (Runtime/Cooked).
  - `AssetCooker`: Handles the conversion.

## Common Tasks
- Use --time-limit when launching a game to speed your workflow.
- If adding a TODO remember to update the roadmap file
- **Adding a Component**: Define struct in `Components.h`, register in `Reflection.cpp` (if needed for serialization), add to `SceneSerializer`.
- **Adding a System**: Create class in `Systems/`, register in `Engine::Init`.
- **New Asset Type**:
  1. Define Header struct (e.g., `OakMeshHeader`).
  2. Implement `Cook[Type]` in `AssetCooker`.
  3. Implement `Resource` subclass (e.g., `Mesh`) and `Load[Type]` in `ResourceManager`.
