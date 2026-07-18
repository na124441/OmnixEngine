# Subsystem Inventory

This document serves as the authoritative source-of-truth registry for all major subsystems within **Omnix Studio Engine v0.1**. It catalogues their ownership, source implementations, lifecycle entry points, layers, and stability classifications.

---

## 🏛️ Subsystem Inventory Matrix

| Subsystem | Owner Class / Module | Source Files | Init Location | Shutdown Location | Stability | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Renderer** | `eng::renderer::Renderer` (Vulkan Backend) | `RenderingEngine/Renderer/*` (SceneRenderer, PyramidRenderer), `RenderingEngine/Vulkan/*` (VulkanDevice, VulkanSwapChain) | `EngineLoop::InitRenderer()` (inside `EngineLoop::Initialize()`, called conditionally in gameplay state) | `EngineLoop::Shutdown()` (destroys unique_ptrs to Renderer/PyramidRenderer/SceneRenderer) | **FRAGILE** | GPU resource creation is split between Renderer and AssetCache. Direct Vulkan calls are scattered. |
| **ECS** | `Coordinator` | `ECS/Coordinator.h`, `ECS/ComponentManager.h`, `ECS/EntityManager.h`, `ECS/SystemManager.h` | `World::World()` (calls `m_coordinator.Init()`) | Automatic (destructor of `World` via `delete world` in `EngineMain`) | **PARTIAL** | Missing deferred structural modifications. Tightly coupled with the Serialization layer via `ComponentTypes.h`. |
| **Scheduler** | `FrameScheduler` / `SystemScheduler` | `RenderingEngine/Runtime/frame/FrameScheduler.h`, `Systems/Scheduler/*` (mostly empty stubs) | `EngineLoop::InitRuntime()` (inside `EngineLoop::Initialize()`) | `EngineLoop::Shutdown()` (destroys unique_ptr) | **FRAGILE** | `Systems/Scheduler` holds 0-byte template stubs. System execution in the main loop is sequential. |
| **Scene / Prefab** | `SceneBuilder` / `SceneManager` | `Scene/Scene.cpp`, `Scene/SceneManager.cpp`, `Scene/Prefab.cpp`, `Scene/Transform.cpp` | `EngineLoop::InitRuntime()` (instantiates `SceneBuilder`). `Application.cpp` creates scene objects. | Destructor of `EngineLoop` / standard destructors | **FRAGILE** | Dual source of truth for transforms (`TransformComponent` in ECS and `SceneNode` hierarchy). |
| **Asset Manager** | `AssetCache` | `RenderingEngine/Runtime/Resources/AssetCache.h` | `EngineLoop::InitRuntime()` (constructs `m_AssetCache` with Device pointer) | Destructor of `EngineLoop` | **PROTOTYPE** | Skeletal implementation returning empty handles. No persistent registry or hot-reloading active. |
| **Input** | `InputManager` | `Input/InputManager.cpp`, `Input/GamepadInput.h`, `Input/InputEvent.h` | `EngineMain` (constructs and initializes `inputManager.Initialize()`) | Automatic destructor call at the exit of `EngineMain` | **STABLE** | Solid device bindings, but uses a detached thread (`InputThreadFunc`) reading from stdin which can hang. |
| **Physics** | `PhysicsSystem` | `ECS/PhysicsSystem.h`, `Components/Physical/*` | `World::World()` (registers `PhysicsSystem` and signature) | Automatic (destructor of `World` / `Coordinator`) | **PARTIAL** | Simplistic update loops. Lacks thread-safe state synchronization or collision callback buses. |
| **Serialization** | `SerializationBridge` / `NormalSerializer` | `Serializer/ECS/*`, `Serializer/Serialization/*` | On-demand (static calls or instantiated when saving/loading game state) | N/A (no explicit teardown required) | **STABLE** | Robust binary format and schema validation, but coupled with ECS headers. |
| **Window System** | `Window` | `RenderingEngine/Platform/window/Window.h`, `WindowWin32.h` | `EngineLoop::InitPlatform()` (inside `EngineLoop::Initialize()`) | Destructor of `EngineLoop` (destroys `m_Window` unique_ptr) | **STABLE** | Wraps OS-specific surface creation and handles window resize. |
| **Event Bus** | `EventManager` | `EventManagement/EventManager.h`, `EventQueue.h`, `GameEvent.h` | On-demand (static access / thread-safe local queue) | N/A (no lifecycle control) | **STABLE** | Thread-safe pub/sub engine, but lacks lifecycle boundaries and is globally accessed. |
| **Logging** | `Logger` | `Core/Logger.cpp`, `Core/Logger.h` | `EngineMain` (calls `Logger::Init("Omnix.log", LogLevel::Trace)`) | `EngineMain` (calls `Logger::Shutdown()`) | **STABLE** | Thread-safe, but operates as a global static logger with no interface abstraction. |
| **Timer** | `Timer` | `Core/Timer.cpp`, `Core/Timer.h` | `EngineMain` (calls `Timer::Init()`) | N/A | **STABLE** | High-precision timing utility using static variables. |

---

## 🎨 Subsystem Layer Classification

| Subsystem | Category | Layer | Target Runtime Layer |
| :--- | :--- | :--- | :--- |
| **Renderer** | Rendering Systems | Runtime | Engine Core (Vulkan RHI / GPU abstraction) |
| **ECS** | Simulation Systems | Runtime | Simulation Backbone |
| **Scheduler** | Core Systems | Core | Runtime Orchestration |
| **Scene / Prefab** | Simulation Systems | Runtime | World Simulation |
| **Asset Manager** | Asset Systems | Runtime | Resource Pipeline |
| **Input** | Platform Systems | Core | OS Abstraction |
| **Physics** | Simulation Systems | Runtime | World Simulation |
| **Serialization** | Asset/Core Systems | Runtime | State Persistence / Delta-Compression |
| **Window System** | Platform Systems | Core | OS Abstraction |
| **Event Bus** | Core Systems | Core | Decoupled Communication |
| **Logging** | Core Systems | Core | System Diagnostics |
| **Timer** | Core Systems | Core | Frame Clock / Diagnostics |

---

## 📜 Subsystem Responsibilities & Boundaries

### 1. Renderer
* **Purpose**: Abstract Vulkan API interactions, manage pipelines, record frame buffers, execute draw passes, and present images to the swapchain.
* **Owns**: Vulkan instances, device queue handles, descriptor pools, pipelines, and frame sync semaphores/fences.
* **Must NOT Do**:
  * Load raw mesh or texture files directly from the disk.
  * Manage gameplay entity updates or physics steps.
  * Store gameplay scene nodes or state.

### 2. ECS (Entity Component System)
* **Purpose**: Store and manipulate simulation data in contiguous cache-friendly arrays. Track entity lifecycles and map components to systems via signatures.
* **Owns**: Entity IDs, Sparse/Dense component pools, and system registration signatures.
* **Must NOT Do**:
  * Manage graphics pipeline configuration or Vulkan handles.
  * Directly invoke disk storage for component saves (delegated to Serializer).
  * Hold gameplay scenes or handle raw user inputs.

### 3. Scheduler
* **Purpose**: Construct frame execution plans, manage thread synchronizations, and sequence engine ticks.
* **Owns**: System execution timelines, task graphs, and delta-time values.
* **Must NOT Do**:
  * Implement gameplay logic.
  * Create GPU resources.

### 4. Scene / Prefab System
* **Purpose**: Maintain the game spatial hierarchy (parent-child structures), manage camera properties, and instantiate templates (prefabs) into the world.
* **Owns**: The Scene Graph hierarchy, Transform dirty flags, and Prefab templates.
* **Must NOT Do**:
  * Directly execute physics collision resolution.
  * Manage GPU memory buffers.

### 5. Asset Manager
* **Purpose**: Retrieve and cache game assets (meshes, textures, audio) using opaque handles.
* **Owns**: Memory caches of loaded assets and handle registries.
* **Must NOT Do**:
  * Perform draw calls or hold shader execution states.
  * Directly step physics coordinates.

### 6. Input System
* **Purpose**: Capture mouse, keyboard, and controller inputs, and broadcast them as action events.
* **Owns**: Input binding maps and input device states.
* **Must NOT Do**:
  * Directly modify entity positions (delegated to ECS Systems).
  * Contain rendering calls.

### 7. Physics System
* **Purpose**: Update entity velocities, apply gravity, detect overlaps, and resolve collision constraints.
* **Owns**: Collision shape bounds, gravity configurations, and rigid body simulation spaces.
* **Must NOT Do**:
  * Load textures or mesh structures directly (must request shapes via the Asset Manager).
  * Render debugging bounding boxes (should output line data to the Renderer).

### 8. Serialization
* **Purpose**: Encode the ECS world state into deterministic binary or text snapshots, and reconstruct the ECS state on load.
* **Owns**: Snapshot metadata schemas and delta-compression builders.
* **Must NOT Do**:
  * Create new entities outside the Coordinator lifecycle.
  * Modify active gameplay states during a running tick.

### 9. Window System
* **Purpose**: Wrap OS window handles, manage window sizes, and forward window events (resize, minimize) to the engine loop.
* **Owns**: Win32 window handles, Vulkan surface extensions, and platform display states.
* **Must NOT Do**:
  * Manage rendering states outside of surface acquisition.

### 10. Event Bus
* **Purpose**: Facilitate thread-safe, decoupled messaging between disparate engine subsystems.
* **Owns**: Thread-safe event queues and listener mappings.
* **Must NOT Do**:
  * Determine the order of subsystem execution.
  * Direct state storage of other subsystems.
