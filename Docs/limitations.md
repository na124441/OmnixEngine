# Omnix Engine Known Limitations & Architectural Debt

This document provides a transparent, engineering-level disclosure of known limitations, bottlenecks, and uncompleted modules in **Omnix Engine v0.4**. This record ensures that systems developers and evaluators understand current architectural trade-offs before deploying or contributing.

---

## 1. Concurrency & Execution Constraints

### 1.1 Single-Threaded Simulation and Command Recording
* **Status**: Concrete Architectural Invariant in v0.4
* **Description**: All gameplay logic, ECS system updates, transform hierarchy calculations, and Vulkan command buffer recording occur exclusively on Thread 0.
* **Impact**: Under high entity counts (>50,000 active ticking entities) or heavy draw call counts (>2,000 non-instanced draw calls), CPU frame time increases linearly. The engine does not utilize available multicore hardware for parallel command recording or parallel ECS chunk updates.
* **Mitigation / Roadmap**: Introduce job-system task dispatching (`WorkerThreadPool`) and parallel Vulkan secondary command buffers (`VkCommandBufferInheritanceInfo`).

### 1.2 Fixed Sleep-Based Frame Pacing
* **Status**: Temporary Pacing Implementation
* **Description**: In [`EngineRuntime.cpp`](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L368), frame rate limiting is currently achieved via `std::this_thread::sleep_for(std::chrono::milliseconds(16))` instead of sub-millisecond precision spin-locks or Vulkan swapchain presentation pacing (`VK_PRESENT_MODE_FIFO_KHR`).
* **Impact**: Frame jitter and inconsistent delta time pacing on displays running at refresh rates other than 60 Hz (e.g., 120 Hz, 144 Hz, or VR displays).

---

## 2. ECS & Memory Limitations

### 2.1 Fixed Entity Capacity (120,000 Maximum Entities)
* **Status**: Hard Upper Limit
* **Description**: `MAX_ENTITIES` is defined as a compile-time constant (`120,000`). `ComponentArray<T>` pre-allocates flat contiguous memory blocks for this maximum capacity.
* **Impact**: Exceeding 120,000 entities triggers an assertion fault. Furthermore, rarely used components waste virtual memory address space because memory is reserved statically per component type rather than allocated in sparse chunks or archetypes.
* **Mitigation / Roadmap**: Transition from Austin Morlan fixed-capacity arrays to sparse-set or chunked archetype tables (e.g., Flecs-style 16KB archetype chunks).

### 2.2 Algorithmic Inefficiency in `EntityManager::DestroyEntity` ($O(N^2)$ Batch Deletion)
* **Status**: Known Complexity Bug
* **Description**: In [`ECS/EntityManager.cpp`](file:///d:/OmnixEngine/ECS/EntityManager.cpp#L44), `DestroyEntity()` removes the entity from `m_LivingEntities` using `std::vector::erase(std::remove(...))`.
* **Impact**: Destroying a single entity is $O(N)$. Destroying $M$ entities sequentially causes $O(M \times N)$ algorithmic complexity. In the 100,000 entity stress test, tearing down all entities takes over 10 minutes due to memory copying.
* **Mitigation**: Change `m_LivingEntities` to an unordered swap-and-pop vector or manage entity liveness strictly through the signature bitset and free queue.

### 2.3 Hash Map Lookups in Component Indexing
* **Status**: Performance Bottleneck
* **Description**: `ComponentArray<T>` maintains entity-to-index mappings via `std::unordered_map<EntityID, size_t>`.
* **Impact**: Dynamic component attachment throughput is capped at ~64,500 attachments/sec due to hash bucket probing and dynamic rehashing.
* **Mitigation**: Replace `std::unordered_map` with a flat sparse array (`EntityID -> size_t` direct lookup table).

---

## 3. Rendering Pipeline Architectural Debt

### 3.1 Monolithic `Renderer.cpp` (5,193 Lines of Code)
* **Status**: High Maintenance Complexity
* **Description**: [`Rendering/Core/Renderer.cpp`](file:///d:/OmnixEngine/Rendering/Core/Renderer.cpp) contains 5,193 lines of code spanning swapchain creation, pipeline layout initialization, 12 render passes, ImGui integration, descriptor management, uniform buffer uploading, and shadow matrix calculations.
* **Impact**: High compilation times, severe risk of side effects during modifications, and violation of the Single Responsibility Principle.
* **Mitigation**: Refactor into modular render passes implementing an abstract `IRenderPass` interface (`GBufferPass`, `LightingPass`, `PostProcessPass`, `ShadowPass`).

### 3.2 Dynamic Descriptor Set Allocation
* **Status**: Suboptimal GPU-CPU Bandwidth
* **Description**: Several rendering passes allocate and update `VkDescriptorSet` instances dynamically per frame rather than utilizing `VK_KHR_push_descriptor` or persistent bindless descriptor arrays (`VK_EXT_descriptor_indexing`).
* **Impact**: Introduces Vulkan CPU descriptor pool lock contention and frequent `vkUpdateDescriptorSets` overhead.

### 3.3 Particle System GPU Buffer Inefficiencies
* **Status**: Unoptimized Compute Integration
* **Description**: [`Rendering/Private/ParticleSystem.cpp`](file:///d:/OmnixEngine/Rendering/Private/ParticleSystem.cpp) features compute shader particle simulation, but particle vertex buffers are staged via host-visible memory rather than maintaining persistent device-local storage buffers with indirect draw arguments (`vkCmdDrawIndirect`).

---

## 4. Incomplete Subsystems (Marked `Planned`)

The following subsystems are present in the directory structure or preliminary headers but are **not fully integrated** into the production frame loop:

| Subsystem | Source Path | Current State | Missing Functionality |
|:---|:---|:---|:---|
| **Skeletal Animation** | `Scene/Components/AnimationComponent.h` | Prototype | Dual-quaternion skinning, animation state blend trees, and root motion extraction are incomplete. Basic keyframe transforms only. |
| **Scripting VM** | `Scripting/` | Planned | Lua / C# runtime bridge is not bound to ECS components. Scripting must currently be authored in native C++ via custom systems. |
| **Network Replication** | `Networking/` | Planned | Binary packet serializers exist, but client-side prediction, delta compression, and authoritative physics reconciliation are unbuilt. |
| **Navigation & Pathfinding** | `AI/NavMesh/` | Planned | Recast/Detour integration is non-functional; no runtime mesh generation or crowd pathfinding. |
| **Material Graph Compiler** | `Tools/MaterialEditor/` | Prototype | Node editor UI exists in ImGui, but GLSL code-generation from visual node graphs is incomplete. Shaders must be authored in raw GLSL and precompiled via `glslc`. |
