# Architecture Audit Report

**Project:** Omnix Studio Engine (v0.1) → v0.2 *Runtime Genesis*  
**Generated:** 2026‑05‑13

---

## 1. Introduction

The purpose of this audit is to enumerate the **dependency graph**, **ownership responsibilities**, and **consumer relationships** of the core engine subsystems.  By exposing these relationships we can detect:
- Circular include dependencies
- Ambiguous ownership of resources (memory, GPU objects, handles)
- Hidden coupling that makes refactoring hazardous
- Unsafe initialization ordering that can cause runtime crashes during engine startup.

The audit focuses on the eight subsystems requested by the user:
- **Renderer**
- **ECS**
- **Scheduler**
- **Scene**
- **Assets**
- **Input**
- **Physics**
- **Serialization**

All conclusions are derived from static code analysis (include chains, header locations, and class responsibilities).

---

## 2. Methodology

1. **File discovery** – Used `glob` to locate header files for each subsystem.
2. **Include extraction** – Ran `grep "#include"` on each header to build a **dependency list**.
3. **Ownership inference** – Inspected class definitions for members that own resources (e.g., `std::unique_ptr`, raw Vulkan handles, asset‑handle maps).
4. **Consumer mapping** – Scanned the codebase for `#include` statements that reference a subsystem’s public headers to determine **"Used By"**.
5. **Cross‑checking** – Compared the dependency and used‑by lists to surface cycles.

---

## 3. Subsystem Matrix

| Subsystem | Depends On (direct includes) | Owns (primary resources) | Used By (other subsystems that #include its public headers) |
|-----------|----------------------------|--------------------------|------------------------------------------------------------|
| **Renderer** | `RenderingEngine/rhi` (Vulkan device, buffers, descriptor sets) <br> `EngineLoop` (frame scheduling) <br> `AssetCache` (mesh/material handles) <br> `Scene` (scene graph) <br> `RenderGraph` (pass ordering) | GPU resources – `VkBuffer`, `VkImage`, `VkPipeline`, descriptor pools, render‑pass objects <br> Render‑graph nodes and pass metadata <br> Scene‑render data (mesh, material, texture references) | `EngineLoop` (drives per‑frame render) <br> `RenderSystem` (queues render commands) <br> `Scene` (provides scene graph) <br> `Asset` system (supplies meshes/materials) |
| **ECS** | `ComponentManager`, `EntityManager`, `SystemManager`, `Serializer/ECS/ComponentTypes.h` (for component‑type IDs), `Logger` | Entity IDs, component pools, component signatures, system registration, entity‑component lifecycle | `World` (exposes Coordinator) <br> `RenderSystem`, `PhysicsSystem`, `PlayerSystem` (query components) <br> `Scene` (transform hierarchy stored as components) |
| **Scheduler** | `SystemManager` (system registration) <br> `Coordinator` (component signatures) <br> `FrameScheduler` (timing) | System‑execution DAG, ordering/dependency information, per‑frame execution plan | `EngineLoop` (calls `FrameScheduler::Execute()`) <br> `World` (processes pending state) <br> **All** systems (register with scheduler) |
| **Scene** | `ECS` (transform, tag, hierarchy components) <br> `AssetCache` (mesh & texture resources) <br> `Renderer` (scene render data structures) | Scene graph nodes, hierarchical transforms, prefab registry, camera objects, serialization data for a scene | `RenderSystem` (reads scene for draw calls) <br> `EngineLoop` (scene loading) <br> `Asset` system (loads meshes/materials) |
| **Assets** | `AssetCache` (registry implementation) <br> `RHI` (creates GPU resources) <br> `Serializer` (metadata persistence) <br> `Input` (hot‑reload notifications) | Asset handles/UUIDs, runtime GPU resources (textures, buffers), metadata tables, hot‑reload queues | `Renderer` (requests textures/meshes) <br> `Scene` (loads meshes, materials) <br> `EngineLoop` (initialises `AssetCache`) <br> `Input` (detects file changes) |
| **Input** | `InputManager` (device abstraction) <br> `EventManagement` (event bus) <br> `EngineLoop` (per‑frame polling) | Keyboard/Mouse/Gamepad state, input bindings, CLI input queue, event dispatch objects | `EngineLoop` (polls each frame) <br> Gameplay systems (e.g., `PlayerController`) |
| **Physics** | `ECS` (entity/component data) <br> `PhysicsSystem` (simulation) <br> `Coordinator` (signatures) <br> `AssetCache` (collision‑shape resources) | Rigid‑body state, collision data, physics world context | `PhysicsSystem` (updates each tick) <br> `RenderSystem` (debug visualisation) <br> `Scene` (static colliders) |
| **Serialization** | `ECS` (component data) <br> `Serializer/ECS` (snapshot types) <br> `AssetCache` (asset IDs) <br> `RHI` (buffer formats) | `ECSSnapshot` binary/text format, delta‑compression logic, versioning headers, checksum utilities | `EngineRuntime` (snapshot / restore) <br> Future networking layer <br> Editor (undo/redo) <br> Asset pipeline (metadata persistence) |

---

## 4. Findings

### 4.1 Circular Dependencies

| Cycle | Files Involved | Why it is problematic |
|-------|----------------|----------------------|
| **Renderer ↔ AssetCache ↔ Serializer ↔ ECS** | `EngineLoop.h` includes `renderer/Renderer.h`, `runtime/resources/AssetCache.h`, `serializer/ECS/ComponentTypes.h`; `AssetCache.h` includes `RHI/RHIDevice.h`, which again pulls in `Renderer`‑related headers. | Changes in any one of these modules force recompilation of the others, preventing independent evolution. It also obscures the true ownership of GPU resources. |
| **ECS ↔ Serializer** | `Coordinator.h` directly includes `../Serializer/ECS/ComponentTypes.h` to obtain component‑type IDs. | Tightly couples the core ECS to the serialization layer, making it difficult to replace or version the serializer without modifying ECS. |

### 4.2 Ownership Confusion

- **GPU Resources**: `Renderer` creates pipelines and descriptor sets, but `AssetCache` also creates `VkImage`/`VkBuffer` for textures and meshes. No single owner is responsible for destruction; both subsystems call `vkDestroy*` in different places, risking double‑free or leaks.
- **Handle Lifetime**: Asset handles (`uint64_t`) are handed to the renderer, yet the renderer never validates that the handle still maps to a live GPU resource.
- **Scene Graph vs. ECS**: Transform data lives both in the ECS (`TransformComponent`) and in the scene hierarchy (`SceneNode`). There is no single source of truth, which can lead to diverging transform states.

### 4.3 Hidden Coupling

- **Event Bus**: `Input` publishes events directly to gameplay systems without an abstraction layer, meaning any change to the event bus API ripples through all consumer systems.
- **Physics ↔ AssetCache**: Collision‑shape loading pulls texture assets directly, a non‑obvious dependency that makes the physics system fragile if the asset pipeline changes.

### 4.4 Unsafe Initialization Order

Current startup (see `EngineLoop::Initialize`):
1. Initialise logging, timers.
2. Construct `EngineLoop` → includes `Renderer`, `AssetCache`, `SceneBuilder`.
3. `World` (and thus `Coordinator`) is created **after** the renderer has already been instantiated.
4. Renderer may request meshes/textures during its first frame, but `AssetCache` may not yet be fully populated, leading to **null handles** or race conditions.

---

## 5. Recommendations (v0.2 Roadmap)

1. **Introduce a clear ownership hierarchy**:
   - **AssetManager** becomes the sole owner of GPU resources (textures, vertex buffers). It exposes **read‑only handles** to the renderer.
   - Renderer should only hold *references* to those handles; destruction is delegated to AssetManager via a deterministic shutdown sequence.
2. **Break the ECS‑Serializer include cycle**:
   - Move component‑type IDs to a **pure header** (`ECS/ComponentTypes.hpp`) that contains only enum definitions, **no serializer code**.
   - Serializer can depend on this header, while ECS remains independent.
3. **Separate the render‑graph from the low‑level RHI**:
   - Create an intermediate **RenderBackend** layer that hides Vulkan specifics from the high‑level renderer. This reduces the coupling between `Renderer` and `RHI`.
4. **Unify Transform ownership**:
   - Choose either **ECS** or **Scene graph** as the single source of transform data. Prefer ECS, and provide a thin **SceneAdapter** that reads/writes `TransformComponent` for editor or legacy code.
5. **Explicit startup sequencing** (e.g., a `EngineRuntime` class):
   - `EngineRuntime::Initialize()` → Logger → AssetManager → RHI → Renderer → World/ECS → Scheduler → MainLoop.
   - Each step verifies that required subsystems are ready before proceeding.
6. **Event Bus abstraction**:
   - Wrap `EventManagement` in an interface (`IEventBus`) and inject it into subsystems (Input, Physics, Gameplay). This decouples event producers from consumers and eases future refactoring.
7. **Add static analysis / include‑graph checks** to the CI pipeline, flagging new cycles automatically.
8. **Document ownership contracts** in the code (e.g., comment blocks in header files) and maintain an **Ownership Matrix** in the project docs.

---

## 6. Conclusion

The audit reveals a **tangled dependency web** that hampers scalability and increases the risk of runtime crashes. By establishing **single owners for resources**, **cleaning up include cycles**, and **formalising the initialization order**, the engine will gain the modularity required for the ambitious v0.2 *Runtime Genesis* goals.

---

*Prepared by OpenCode – your collaborative AI engineer.*