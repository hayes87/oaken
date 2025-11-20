# Oaken Engine Roadmap

## Phase 1: Core Architecture (Completed)
- [x] **Modular Architecture**: Split Engine and Game into separate DLLs.
- [x] **Hot Reloading**: Implemented `Runner` executable to reload Game DLL on the fly.
- [x] **Build System**: CMake setup with automatic dependency management (vcpkg) and DLL copying.

## Phase 2: Asset Pipeline (Completed)
- [x] **Asset Pre-processing (Cooking)**:
    - [x] **Architecture**: Separate `AssetCooker` process.
    - [x] **Atomic Writes**: Cooker writes to `.tmp` and renames to prevent locking.
    - [x] **Texture Cooking**: Load `.png`/`.jpg` and write custom `.oaktex` binary format.
- [x] **Hot Reloading Pipeline**:
    - [x] **Launcher**: File watcher detects changes and triggers Cooker.
    - [x] **Generic Reloading**: Engine detects cooked file changes and reloads resources at runtime.
    - [x] **Runtime Update**: Textures update in-game without restarting.

## Phase 3: Rendering Foundation (Current Focus)
- [ ] **Batch Rendering**:
    - [ ] Implement `SpriteBatch` to group draw calls.
    - [ ] Optimize `RenderSystem` to use a single vertex buffer for multiple sprites.
- [ ] **Mesh Support**:
    - [ ] Implement `CookMesh` in AssetCooker (glTF -> .oakmesh).
    - [ ] Implement `Mesh` resource class and `Reload()` logic.
    - [ ] Render 3D meshes in `RenderSystem`.

## Phase 4: Gameplay Systems (Next Up)
- [ ] **Physics Integration (Jolt)**:
    - [ ] Initialize Jolt Physics system (currently a placeholder).
    - [ ] Create `RigidBody` and `Collider` components.
    - [ ] Implement `PhysicsSystem::Step` to sync ECS Transforms <-> Jolt Bodies.
    - [ ] Debug Drawing for colliders.
- [ ] **Scripting (Lua)**:
    - [ ] Initialize Lua state in `ScriptSystem` (currently a placeholder).
    - [ ] Bind Core Types: `Entity`, `Transform`, `Vector3`.
    - [ ] Implement `ScriptComponent` to attach `.lua` files to entities.
    - [ ] Execute `OnUpdate` in Lua scripts.

## Phase 5: Editor & Tools
- [ ] **Entity Inspector**:
    - [ ] Hierarchy Panel: List all entities in the ECS world.
    - [ ] Properties Panel: View and edit component data (Transform, etc.) using ImGui.
    - [ ] Selection Handling: Click to select entities.
- [ ] **Asset Browser**:
    - [ ] View files in the project directory.
    - [ ] Drag-and-drop assets into the scene.
- [ ] **Gizmos**:
    - [ ] Visual tools to move/rotate/scale entities in the viewport (using `ImGuizmo`).

## Phase 6: Scene Management
- [ ] **Serialization**:
    - [ ] Save ECS World to JSON/Binary (Scene files).
    - [ ] Load Scene files on startup.
- [ ] **Scene Graph**:
    - [ ] Parent/Child relationships in ECS (Flecs supports this natively).
    - [ ] Transform propagation (Parent moves -> Child moves).

## Phase 7: Rendering (Advanced)
- [ ] **Material System**:
    - [ ] Shader reflection to generate UI for material properties.
- [ ] **Lighting**:
    - [ ] Directional, Point, and Spot lights.
    - [ ] PBR (Physically Based Rendering) pipeline.
- [ ] **Post Processing**:
    - [ ] Bloom, Tone Mapping, Color Correction.

