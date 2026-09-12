# OMNIX ENGINE — PHASE-WISE REMEDIATION & FIX PLAN
**Target Repository:** `https://github.com/na124441/OmnixEngine/`  
**Working Directory:** `d:\OmnixEngine`  
**Reference Document:** [`OMNIX_PRE_REFACTOR_FORENSIC_AUDIT.md`](file:///d:/OmnixEngine/OMNIX_PRE_REFACTOR_FORENSIC_AUDIT.md)  
**Status:** Completed  
**Current Phase:** All Phases (0 - 3) Completed — Engine Status: GREEN  

---

## 1. Objectives & Guiding Principles
This remediation plan translates the forensic audit findings into a strictly ordered, phase-by-phase execution roadmap. The goal is to transition Omnix Engine from its high-risk state (**ORANGE**) to a stable, clean, unified, interview-ready 3D game engine (**GREEN**).

### Core Architectural Decisions
1. **Remove Blocking Stdin Console Thread:** Synchronous standard input (`std::getline`) attached to a detached background thread hangs process termination under Windows. Console output will route through standard stdout/stderr and an in-engine ImGui debug log panel.
2. **Standardize on Canonical ECS:** [`ECS/Coordinator.h`](file:///d:/OmnixEngine/ECS/Coordinator.h) is the single source of truth for entity-component data. The duplicate implementation in [`Serializer/ECS/`](file:///d:/OmnixEngine/Serializer/ECS/) and uncompiled prototype [`RenderingEngine/Runtime/World/World.cpp`](file:///d:/OmnixEngine/RenderingEngine/Runtime/World/World.cpp) will be eliminated.
3. **Unify Scene and ECS:** Redefine [`SceneObject`](file:///d:/OmnixEngine/Scene/SceneObject.h) as a lightweight handle wrapping an entity ID. Delete the $O(N^2)$ brute-force synchronization loop in [`SceneManager::SyncECSToScene()`](file:///d:/OmnixEngine/Scene/SceneManager.cpp#L290).
4. **Built-in Procedural Fallbacks:** Provide programmatic 3D primitives (unit cube, plane/quad) in `AssetManager` so that missing external asset files do not crash the engine or abort scene validation.

---

## 2. Phase-Wise Execution Overview

```
+---------------------------------------------------------------+
| PHASE 0: RUNTIME STABILIZATION & CLEAN PROCESS EXIT           |
| - Kill detached stdin thread (Fix shutdown hang)              |
| - Reorder shutdown calls (Silence false-positive exit error)  |
| - Guard swapchain recreation against 0x0 minimize crashes     |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| PHASE 1: REPOSITORY HYGIENE & ASSET PIPELINE UNBLOCKING       |
| - Untrack committed .o and .exe binaries; update .gitignore   |
| - Delete 0-byte ghost headers & uncompiled World.cpp          |
| - Replace AssetCache(nullptr) with valid AssetManager         |
| - Implement procedural fallback cube mesh                     |
| - Make SceneValidator non-fatal for missing optional assets   |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| PHASE 2: INPUT SYSTEM & SCENE/ECS UNIFICATION                 |
| - Connect GLFW mouse callbacks to MouseInput                  |
| - Decouple PlayerControllerSystem from direct ImGui polling   |
| - Strip SceneObject into a thin entity handle                 |
| - Delete O(N^2) SyncECSToScene loop                           |
| - Deprecate and remove Serializer/ECS redundant tree          |
+-------------------------------+-------------------------------+
                                |
                                v
+---------------------------------------------------------------+
| PHASE 3: GOLDEN DEMO VERTICAL SLICE & VERIFICATION            |
| - Verified 3D startup scene (Ground plane, lit cube, camera)  |
| - Standard WASD + Mouse-look free camera                      |
| - 60+ FPS viewport rendering                                  |
| - Clean, 0-warning exit <100ms with exit code 0               |
+---------------------------------------------------------------+
```

---

## 3. Detailed Phase Breakdown

### Phase 0: Runtime Stabilization & Clean Process Exit

#### Problem Statements
- `EngineRuntime::InputThreadWorker` runs a blocking `std::getline(std::cin, line)` on a dedicated thread that is detached on shutdown (`m_InputThread.detach()`), hanging the process indefinitely on exit.
- `OwnershipValidator` asserts LIFO shutdown. Subsystems are torn down in an order that violates registration sequence, generating a spurious `SHUTDOWN ORDER VIOLATION` error on every clean exit.
- In `EngineLoop::RecreateSwapChain()`, minimizing the window causes width/height to drop to 0. Swapchain framebuffers are destroyed and rendering continues into an empty list, triggering Vulkan validation errors.

#### Specific Code Actions
1. **[MODIFY] [`Runtime/Public/EngineRuntime.h`](file:///d:/OmnixEngine/Runtime/Public/EngineRuntime.h)**
   - Remove `std::thread m_InputThread;`, `std::atomic<bool> m_InputThreadRunning;`, and `void InputThreadWorker();`.
2. **[MODIFY] [`Runtime/Private/EngineRuntime.cpp`](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp)**
   - Remove `InputThreadWorker()` implementation and thread spawning in `Initialize()`.
   - Reorder `EngineRuntime::Shutdown()` teardown sequence to match the exact reverse of `Initialize()`:
     ```
     Startup:  Input(1) -> Events(2) -> ECS(3) -> Scheduler(4) -> Renderer(5) -> PhysicsWorld(6) -> Assets(7) -> Scene(8) -> WorldManager(9) -> Audio(10) -> Editor(11)
     Shutdown: Editor(11) -> Audio(10) -> WorldManager(9) -> Scene(8) -> Assets(7) -> PhysicsWorld(6) -> Renderer(5) -> Scheduler(4) -> ECS(3) -> Events(2) -> Input(1)
     ```
3. **[MODIFY] [`RenderingEngine/Runtime/engine/EngineLoop.cpp`](file:///d:/OmnixEngine/RenderingEngine/Runtime/engine/EngineLoop.cpp)**
   - In `RecreateSwapChain()`, replace the early return on 0 width/height with a message-pumping wait loop that sleeps until valid window dimensions are restored or the window is closed:
     ```cpp
     while (width == 0 || height == 0) {
         auto result = m_Window->PollEvents();
         if (result.IsFailure()) {
             m_Running.store(false, std::memory_order_relaxed);
             return;
         }
         width = m_Window->GetWidth();
         height = m_Window->GetHeight();
         std::this_thread::sleep_for(std::chrono::milliseconds(16));
     }
     ```

#### Verification Criteria
- Launch engine, close GLFW window. Verify process terminates cleanly within 100ms with exit code 0.
- Verify stdout/stderr has zero `SHUTDOWN ORDER VIOLATION` log messages.
- Minimize window during rendering and restore; verify no crash or Vulkan validation error occurs.

---

### Phase 1: Repository Hygiene & Asset Pipeline Unblocking

#### Problem Statements
- Object files (`EditorLayer.o`, `FieldSnapshot.o`, `PhysicsDebugDraw.o`, `SceneObject.o`, `SceneRenderer.o`) and executables (`test.exe`, `test_compile.exe`) are committed in Git.
- 0-byte ghost headers exist in [`Systems/Types/AI-Cognitive/`](file:///d:/OmnixEngine/Systems/Types/).
- `EngineRuntime::Initialize()` passes `nullptr` into `AssetCache`, breaking dynamic asset loading.
- `AssetRegistry.json` references missing files (`cube.obj`, `pyramid.obj`, `brick_albedo.png`), causing `SceneValidator` to reject scenes and prevent them from loading.

#### Specific Code Actions
1. **[GIT PURGE]**
   - Untrack all `.o` and `.exe` files from Git: `git rm --cached *.o *.exe`.
   - Update `.gitignore` to ignore `*.o`, `*.obj`, `*.exe`, `*.pdb`, `*.ilk`, `*.bin`.
2. **[DELETE DEAD CODE]**
   - Remove 0-byte headers:
     - `Systems/Types/AI-Cognitive/ImitationLearning.h`
     - `Systems/Types/AI-Cognitive/PolicyEvaluation.h`
     - `Systems/Types/AI-Cognitive/RewardShaping.h`
     - `Systems/Types/AI-Cognitive/RLHooks.h`
     - `Systems/Types/AI-Cognitive/ValueIteration.h`
   - Delete uncompiled duplicate [`RenderingEngine/Runtime/World/World.cpp`](file:///d:/OmnixEngine/RenderingEngine/Runtime/World/World.cpp).
   - Remove abandoned `int EngineMain()` in [`Core/Application.cpp`](file:///d:/OmnixEngine/Core/Application.cpp).
3. **[MODIFY] [`Runtime/Private/EngineRuntime.cpp`](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp) & [`Runtime/Private/AssetManager.cpp`](file:///d:/OmnixEngine/Runtime/Private/AssetManager.cpp)**
   - Connect `AssetManager` properly to `AssetCache`.
   - Add procedural fallback geometry generation (`builtin://cube`, `builtin://plane`) inside `AssetManager`.
4. **[MODIFY] [`Scene/SceneValidator.cpp`](file:///d:/OmnixEngine/Scene/SceneValidator.cpp)**
   - Downgrade missing mesh/texture references from fatal validation aborts to warnings that substitute procedural fallback primitives.

#### Verification Criteria
- `git status` shows zero committed binaries or ghost headers.
- Engine boots and loads default sample scene without throwing asset-not-found exceptions.
- Viewport displays a rendered procedural cube rather than an empty black screen.

---

### Phase 2: Input System & Scene/ECS Unification

#### Problem Statements
- `MouseInput::UpdateState()` and `GamepadInput::UpdateState()` are empty stubs.
- `PlayerControllerSystem` interrogates `ImGui::IsKeyDown()` directly, breaking in release/game-only builds and conflicting with editor text input.
- `SceneObject` duplicates ~25 component variables and uses its own transform. `SceneManager::SyncECSToScene()` runs an $O(N^2)$ brute-force linear search over all entities to synchronize them.
- A redundant parallel ECS exists in [`Serializer/ECS/`](file:///d:/OmnixEngine/Serializer/ECS/).

#### Specific Code Actions
1. **[MODIFY] [`Input/MouseInput.h`](file:///d:/OmnixEngine/Input/MouseInput.h) & [`Input/InputManager.cpp`](file:///d:/OmnixEngine/Input/InputManager.cpp)**
   - Hook GLFW mouse callbacks (`glfwSetCursorPosCallback`, `glfwSetMouseButtonCallback`, `glfwSetScrollCallback`) into `MouseInput`.
   - Implement `GetMousePosition()`, `GetMouseDelta()`, and `IsMouseButtonDown()`.
2. **[MODIFY] [`ECS/PlayerControllerSystem.h`](file:///d:/OmnixEngine/ECS/PlayerControllerSystem.h)**
   - Decouple from ImGui. Route player movement (WASD) and mouse look through `InputManager`.
3. **[MODIFY] [`Scene/SceneObject.h`](file:///d:/OmnixEngine/Scene/SceneObject.h) & [`Scene/SceneObject.cpp`](file:///d:/OmnixEngine/Scene/SceneObject.cpp)**
   - Remove duplicated boolean flags (`m_HasStaticBody`, `m_HasBoxCollider`, etc.) and duplicate transforms.
   - Refactor `SceneObject` into a thin wrapper around `Entity` ID that forwards component queries to `Coordinator`.
4. **[MODIFY] [`Scene/SceneManager.cpp`](file:///d:/OmnixEngine/Scene/SceneManager.cpp)**
   - Delete `SyncECSToScene()` and its $O(N^2)$ entity search loop.
5. **[DELETE] [`Serializer/ECS/`](file:///d:/OmnixEngine/Serializer/ECS/)**
   - Remove redundant parallel ECS implementation.

#### Verification Criteria
- Mouse look and WASD translation work smoothly in the viewport.
- Modifying entity transforms in physics or gameplay immediately reflects in rendering without calling `SyncECSToScene`.
- 10,000 entities can be spawned without frame-rate collapse from $O(N^2)$ synchronization.

---

### Phase 3: Golden Demo Vertical Slice & Polish

#### Goal
Deliver an undeniable, verified 3D vertical slice that proves all core engine systems work harmoniously together.

#### Deliverables
1. **Showcase 3D Scene:**
   - Static ground plane with physics collider and checkerboard/concrete material.
   - Lit procedural cube with active Jolt rigid body simulation.
   - Controllable player/camera entity.
2. **Interactive Controls:**
   - WASD to translate camera in world space.
   - Mouse delta to pitch and yaw the camera.
   - Space to jump or apply impulse to physics bodies.
3. **Stability & Performance:**
   - Stable 60+ FPS in Vulkan viewport.
   - ImGui docking panels (Scene Hierarchy, Inspector, Diagnostics) fully reactive.
   - Process exits in under 100ms with exit code 0 and zero error logs upon closing the window.

---

## 4. Progress Tracker

| Task | Phase | Status | Notes |
| :--- | :--- | :--- | :--- |
| Remove detached stdin thread | Phase 0 | **Completed** | Removed from `EngineRuntime.h/.cpp` |
| Reorder shutdown sequence | Phase 0 | **Completed** | Fixed exact reverse order in `EngineRuntime.cpp` |
| Guard swapchain recreation against 0x0 | Phase 0 | **Completed** | Added wait/poll loop in `EngineLoop.cpp` |
| Purge committed `.o` and `.exe` binaries | Phase 1 | **Completed** | Untracked via `git rm --cached`, updated `.gitignore` |
| Delete 0-byte AI ghost headers & dead `World.cpp` | Phase 1 | **Completed** | Deleted ghost headers & uncompiled `World.*` |
| Procedural fallback cube & non-fatal `SceneValidator` | Phase 1 | **Completed** | Added `builtin://cube`, `builtin://plane`, relaxed `SceneValidator` |
| Wire GLFW mouse callbacks | Phase 2 | **Completed** | Mouse position, delta, scroll & buttons wired in `InputManager` |
| Decouple `PlayerControllerSystem` from ImGui | Phase 2 | **Completed** | Uses `InputManager` + Win32 async fallback, ImGui secondary |
| Strip `SceneObject` to entity handle | Phase 2 | **Completed** | Removed 22 duplicate bool flags and fields; routes to `Coordinator` |
| Delete $O(N^2)$ `SyncECSToScene` | Phase 2 | **Completed** | Removed brute-force loop; synchronous sync via `EditorSceneService` |
| Golden Demo Scene & Verification | Phase 3 | **Completed** | Default showcase 3D scene (plane, cube, lights, camera) verified |

