# Subsystem Dependencies & Circular Include Audit

This document defines the compile-time and runtime dependencies of the engine subsystems, maps out the include chains, analyzes circular dependency loops, and provides concrete remediation strategies.

---

## 🔗 Subsystem Dependency Matrix

| Subsystem | Direct Dependencies (Compile-Time / Header) | Runtime Dependencies |
| :--- | :--- | :--- |
| **Renderer** | `rhi` (abstract), Vulkan SDK, GLFW, `Core/World.h` (for data structures), `AssetCache` (mock header) | OS Window context, Vulkan Swapchain |
| **ECS** | `ECSConfig.h`, `Serializer/ECS/ComponentTypes.h` | None (pure simulation backend) |
| **Scheduler** | `FrameContext.h`, `FrameMetrics.h` | SystemManager, Coordinator |
| **Scene / Prefab** | `ECS/Coordinator.h`, `ECS/ECSComponents.h` | `AssetCache` (for mesh/texture IDs), `World` |
| **Asset Manager** | `RHI/RHIDevice.h` (for Device handle) | GPU memory allocator |
| **Input** | `EventManagement/GameEvent.h` (decoupled publishing) | None |
| **Physics** | `ECS/ECSComponents.h` | `Coordinator` iteration loop |
| **Serialization** | `Serializer/ECS/ECS.h`, `Serializer/Serialization/ISerializer.h` | `ECS` singleton instance, file system |
| **Window System** | SDL3 (or platform Win32 SDK) | Vulkan SDK (surface extension loading) |
| **Event Bus** | None (standard template library) | None |
| **Logging** | None (STL stream utilities) | None |
| **Timer** | None | OS High-performance counters |

---

## 🚨 Circular Dependency Audit

We have identified two major circular dependency loops that violate clean software architecture guidelines.

### Cycle 1: Renderer ↔ AssetCache ↔ Serializer ↔ ECS
* **Involved Files**:
  * `EngineLoop.h` includes `renderer/Renderer.h`, `runtime/resources/AssetCache.h`, `serializer/ECS/ComponentTypes.h`.
  * `AssetCache.h` includes `RHI/RHIDevice.h` (RHI Device representation).
  * `VulkanDevice.h` (implements RHI Device) depends on `renderer/Renderer.h` or RHI structures which pull in high-level renderer headers.
* **Why it is problematic**:
  * Any change in the component types or RHI structures triggers a cascading compile across all modules.
  * Ownership of GPU assets is fuzzy: does the `Renderer` own the shaders and pipeline buffers, or does the `AssetCache` own them?
  * Prevents the RHI or AssetCache from being compiled or tested in isolation.

### Cycle 2: ECS ↔ Serializer
* **Involved Files**:
  * `ECS/Coordinator.h` includes `../Serializer/ECS/ComponentTypes.h`.
  * `Serializer/ECS/ComponentTypes.h` includes `Component.h` (`Serializer/ECS/Component.h`), which implements component pools duplicate definitions.
* **Why it is problematic**:
  * The gameplay ECS (`ECS/`) is compile-time linked to the Serializer's schema tracking headers.
  * Tightly couples the simulation layer to the persistence layer, making it impossible to change component databases or version the serializer without modifying the core ECS Coordinator.

---

## 🎯 Target One-Directional Dependency Flow

To eliminate circular dependencies and establish clean architectural layers in **Omnix v0.2**, the compile-time and link-time dependencies must flow strictly from the high-level systems down to the low-level systems as follows:

```
[Gameplay Application]
       │
       ▼
 [Scene / Prefab]
       │
       ▼
     [ECS] ◄────────────── [Physics / Systems]
       │
       ▼
 [Serializer]
       │
       ▼
   [Renderer] (High-Level passes, FrameGraph)
       │
       ▼
 [Asset Manager]
       │
       ▼
 [Vulkan RHI] (Backend implementations)
       │
       ▼
[Window / Platform] (SDL3 / Win32)
       │
       ▼
 [Event Bus / Core Utilities] (Logger, Timer)
```

---

## 🛠️ Proposed Cycle Elimination Strategies

### 1. Breaking the ECS ↔ Serializer Cycle
* **Action**:
  * Move the `ComponentTypeID` enums from `Serializer/ECS/ComponentTypes.h` into a pure, lightweight header `ECS/ComponentTypes.h` in the core ECS module.
  * This header should have **zero includes** of serializer types.
  * The `ECS/Coordinator.h` will include `ECS/ComponentTypes.h`.
  * The `Serializer` will depend on `ECS/ComponentTypes.h` for serialization, but the core ECS will no longer depend on any serializer headers.

### 2. Breaking the Renderer ↔ AssetCache ↔ RHI Cycle
* **Action**:
  * Extract a pure virtual Render Hardware Interface (**RHI**) as a separate library (`RHI`).
  * Subsystems like `AssetCache` or high-level renderers depend only on the RHI interface headers (e.g., `IRHIDevice.h`, `IRHIBuffer.h`), never on Vulkan-specific implementation headers (such as `VulkanDevice.h` or `VulkanSwapChain.h`).
  * Implement dependency injection: pass the concrete `VulkanDevice` pointer as an `IRHIDevice*` interface during initialization.
