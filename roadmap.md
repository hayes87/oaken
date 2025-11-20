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

## Phase 3: Scene System (Foundation)
- [ ] **Scene Architecture**:
    - [ ] Create `Scene` class to manage entities and systems.
    - [ ] Implement `SceneManager` to handle scene switching (e.g., MainMenu -> GameLevel).
- [ ] **Transform Hierarchy**:
    - [ ] Split `Transform` into `LocalTransform` and `WorldTransform`.
    - [ ] Implement `TransformSystem` to propagate changes from Parent to Children (Scene Graph).
- [ ] **Serialization**:
    - [ ] **Text Format (Dev)**: Integrate `nlohmann-json` for human-readable `.oakscene` files (Git-friendly).
    - [ ] **Binary Format (Runtime)**: Implement `CookScene` to convert JSON -> Binary for fast loading.
    - [ ] **Serializer**: Implement `SceneSerializer` to handle both formats.

## Phase 4: 3D Character Pipeline (Priority)
- [ ] **Mesh Support (Static)**:
    - [ ] **Import (Assimp)**: Implement `CookMesh` in AssetCooker using Assimp (glTF/FBX -> .oakmesh).
    - [ ] Implement `Mesh` resource class and `Reload()` logic.
    - [ ] Render static 3D meshes in `RenderSystem`.
- [ ] **Skeletal Animation (ozz-animation)**:
    - [ ] **Import**: Extract Skeleton and Animation Clips using Assimp/ozz-animation tools.
    - [ ] **Runtime**: Integrate `ozz-animation` for playback and sampling.
    - [ ] **Skinning**: Implement Vertex Shader skinning (GPU-based bone transforms).
- [ ] **Camera System**:
    - [ ] **Component**: Create `Camera` component (FOV, Near/Far, Perspective/Ortho).
    - [ ] **System**: Implement `CameraSystem` to manage View/Projection matrices.
    - [ ] **Controller**: Implement `CameraFollow` logic (Third-person orbit/chase).
- [ ] **Character Controller**:
    - [ ] Implement `CharacterController` component (Velocity, State).
    - [ ] Map Input to Character Movement.

## Phase 5: Core Systems & Gameplay (Next Up)
- [ ] **Multithreading Foundation**:
    - [ ] **Job System**: Implement a task scheduler for parallel execution (required for Jolt/AI).
    - [ ] **Thread Safety**: Ensure `ResourceManager` and `EventBus` are thread-safe.
    - [ ] **ECS Multithreading**: Configure Flecs worker threads and component locking.
- [ ] **Physics Integration (Jolt)**:
    - [ ] Initialize Jolt Physics system (utilizing the Job System).
    - [ ] Create `RigidBody` and `Collider` components (Capsule for Character).
    - [ ] Implement `PhysicsSystem::Step` to sync ECS Transforms <-> Jolt Bodies.
    - [ ] Implement Character Virtual Controller (Jolt feature for responsive movement).
- [ ] **AI & Navigation**:
    - [ ] **Navigation Mesh**: Generate NavMesh from level geometry (Recast).
    - [ ] **Pathfinding**: Implement A* or Detour for path calculation.
    - [ ] **Behavior**: Implement Behavior Trees for AI logic.
- [ ] **Scripting (Lua)**:
    - [ ] Initialize Lua state in `ScriptSystem` (currently a placeholder).
    - [ ] Bind Core Types: `Entity`, `Transform`, `Vector3`.
    - [ ] Implement `ScriptComponent` to attach `.lua` files to entities.
    - [ ] Execute `OnUpdate` in Lua scripts.

## Phase 5: Rendering Optimization & Polish
- [ ] **Batch Rendering**:
    - [ ] Implement `SpriteBatch` to group 2D draw calls (UI/Sprites).
    - [ ] Optimize `RenderSystem` to use a single vertex buffer for multiple sprites.
- [ ] **Material System**:
    - [ ] Shader reflection to generate UI for material properties.
- [ ] **Lighting**:
    - [ ] Directional, Point, and Spot lights.
    - [ ] PBR (Physically Based Rendering) pipeline.

## Phase 6: Editor & Tools
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

