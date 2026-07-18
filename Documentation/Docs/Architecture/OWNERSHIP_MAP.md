# Subsystem Ownership & Resource Lifetime Map

This document maps out the creation, lifetime, destruction, and sharing rules for all engine subsystems and primary resources under the Week 4 Explicit Ownership & Runtime Context model.

---

## 🏛️ Subsystem Ownership Matrix

| Subsystem | Who Creates It? | Who Owns It? | Who Destroys It? | Lifetime | Shared? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **EngineRuntime** | `main.cpp` (stack) | `main.cpp` | `main.cpp` (automatic) | Program Duration | Yes (holds the RuntimeContext) |
| **Renderer** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (accessible via Context) |
| **ECS (World)** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (accessible via Context) |
| **Scheduler** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (accessible via Context) |
| **Scene Manager** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (accessible via Context) |
| **Asset Manager** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (accessible via Context) |
| **Input** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (accessible via Context) |
| **Schema Registry** | `EngineRuntime` | `EngineRuntime` (`std::unique_ptr`) | `EngineRuntime::Shutdown()` | Gameplay Loop | Yes (injected where needed) |
| **Logging** | `main.cpp` | Static program space | `main.cpp` (explicit Shutdown) | Program Duration | Yes (thread-safe console/file) |
| **Timer** | `main.cpp` / `EngineRuntime` | Static program space | Process exit | Program Duration | Yes (global state) |

---

## 🛡️ Key Resource Ownership Flows

### 1. Vulkan/GPU Resource Ownership
```
EngineRuntime
 └── m_Renderer (eng::runtime::EngineLoop)
      └── m_RHI (eng::rhi::Device)
           ├── VulkanInstance
           ├── VulkanSwapChain
           └── VulkanDevice
```
* **Vulkan Instance & Device**: Strictly owned by the `m_Renderer` subsystem.
* **Allocation and Deallocation**: GPU buffers and images are safely allocated and destroyed under the authority of `EngineLoop::Shutdown()`, preventing leak hazards.

### 2. Gameplay Simulation State Ownership
```
EngineRuntime
 ├── m_ECS (World)
 │    └── m_coordinator (Coordinator)
 │         ├── EntityManager
 │         ├── ComponentManager (pools)
 │         └── SystemManager
 └── m_Scenes (SceneManager)
```
* **World Lifetime**: Unlike the prototype version where `StateMachine` repeatedly deleted and reallocated `World`, the gameplay world is owned by `EngineRuntime`.
* **Renderer Integration**: `EngineLoop` observes the world via a non-owning pointer `m_World` (injected via `SetExternalWorld`), removing double-deletion hazards.

---

## ⚠️ Lifetime Rules & Validation

1. **Destruction Authority**: Only the direct parent in the ownership hierarchy may delete or reset a subsystem. Subsystems must never call `delete` on raw pointers injected into them.
2. **Reverse Destruction Order**: Subsystems are destroyed on shutdown in the exact reverse order of their startup initialization to ensure dependencies remain valid during teardown.
3. **Lifecycle Validation**: Dynamic lifecycle assertions in `OwnershipValidation.cpp` verify that startup and shutdown orders match the authoritative specification precisely.
4. **Memory Leak Diagnostics**: Allocation metrics in `AllocationDiagnostics.cpp` track subsystem initialization sizes and report leaks on shutdown.
