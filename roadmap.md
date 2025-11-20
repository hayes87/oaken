# Oaken Engine Roadmap

## Phase 1: Core Architecture (Completed)
- [x] **Modular Architecture**: Split Engine and Game into separate DLLs.
- [x] **Hot Reloading**: Implemented `Runner` executable to reload Game DLL on the fly.
- [x] **Build System**: CMake setup with automatic dependency management (vcpkg) and DLL copying.

## Phase 2: Editor & Tools (Current Focus)
- [ ] **Entity Inspector**:
    - [ ] Hierarchy Panel: List all entities in the ECS world.
    - [ ] Properties Panel: View and edit component data (Transform, etc.) using ImGui.
    - [ ] Selection Handling: Click to select entities.
- [ ] **Asset Browser**:
    - [ ] View files in the project directory.
    - [ ] Drag-and-drop assets into the scene.
- [ ] **Gizmos**:
    - [ ] Visual tools to move/rotate/scale entities in the viewport (using `ImGuizmo`).

## Phase 3: Asset Management (Next Up)
- [ ] **Asset Pre-processing (Cooking)**:
    - [x] **Architecture**: Separate `AssetCooker` process.
    - [x] **Atomic Writes**: Cooker writes to `.tmp` and renames to prevent locking.
    - [x] **Texture Cooking**:
        - [x] Load `.png`/`.jpg` using `stb_image`.
        - [x] Write custom `.oaktex` binary format (Header + Pixels).
    - [ ] **Mesh Cooking**:
        - [ ] Load `.gltf` using `fastgltf`.
        - [ ] Write custom `.oakmesh` binary format.
- [ ] **Asset Bundling**:
    - [ ] Pack processed assets into large binary archives (Asset Bundles) for faster IO.
    - [ ] Virtual File System to read from Bundles or loose files (for dev iteration).
- [ ] **Resource Manager**:
    - [x] Basic `ResourceManager` class.
    - [x] Safe `ReadFile` (Read-Close pattern).
    - [x] **Texture Resource**:
        - [x] `Texture` class inheriting from `Resource`.
        - [x] Load `.oaktex` files and upload to SDL3 GPU.
    - [ ] **Mesh Resource**:
        - [ ] `Mesh` class inheriting from `Resource`.
        - [ ] Load `.oakmesh` files and upload to SDL3 GPU.
    - [ ] Async loading support.
    - [ ] Hot Reloading (File Watcher).

## Phase 4: Scene Management
- [ ] **Serialization**:
    - [ ] Save ECS World to JSON/Binary (Scene files).
    - [ ] Load Scene files on startup.
- [ ] **Scene Graph**:
    - [ ] Parent/Child relationships in ECS (Flecs supports this natively).
    - [ ] Transform propagation (Parent moves -> Child moves).

## Phase 5: Physics Integration
- [ ] **Jolt Physics Setup**:
    - [ ] Initialize Jolt Physics system.
    - [ ] Create `RigidBody` and `Collider` components.
- [ ] **Physics System**:
    - [ ] Sync ECS Transform -> Jolt Body (Pre-Physics).
    - [ ] Step Simulation.
    - [ ] Sync Jolt Body -> ECS Transform (Post-Physics).
- [ ] **Debug Drawing**:
    - [ ] Draw wireframe colliders for debugging.

## Phase 6: Scripting (Lua)
- [ ] **Lua Integration**:
    - [ ] Bind ECS functions to Lua (Create Entity, Add Component).
    - [ ] Bind Math types (Vector3, etc.).
- [ ] **Script Component**:
    - [ ] Attach `.lua` scripts to entities.
    - [ ] Call `OnStart`, `OnUpdate` from C++.

## Phase 7: Rendering (Advanced)
- [ ] **Material System**:
    - [ ] Shader reflection to generate UI for material properties.
- [ ] **Lighting**:
    - [ ] Directional, Point, and Spot lights.
    - [ ] PBR (Physically Based Rendering) pipeline.
- [ ] **Post Processing**:
    - [ ] Bloom, Tone Mapping, Color Correction.
