# Deterministic Shutdown Order

This document defines the destruction sequence of **Omnix Studio Engine**. To prevent use-after-free assertions, Vulkan device lost errors, or memory leaks, subsystems must be torn down in the exact reverse order of their initialization.

---

## 🏛️ Shutdown Sequence

```
[Start Teardown]
       │
       ▼
 1. [Input Thread] (Stop and join/detach CLI thread)
       │
       ▼
 2. [Scene System] (Release scene references)
       │
       ▼
 3. [Assets Cache] (Release cached mesh/texture GPU data)
       │
       ▼
 4. [Renderer]     (Destroy Vulkan Swapchain, Pipelines, Fences, and Semaphores)
       │
       ▼
 5. [ECS World]    (Deallocate entity registries and component arrays)
       │
       ▼
 6. [Scheduler]    (Join task worker threads)
       │
       ▼
 7. [Input Manager] (Clear device bindings)
       │
       ▼
 8. [Logging]      (Flush buffers and close trace logs)
       │
       ▼
[Shutdown Complete]
```

---

## ⚠️ Subsystem Destruction Contract

1. **Reverse Execution**: The shutdown pipeline in `EngineRuntime::Shutdown()` processes teardown in the exact reverse sequence of startup.
2. **Double-Teardown Guards**: Every subsystem class must implement a status guard (`m_Initialized`) to prevent accidental double-free errors if a subsystem is deleted twice.
3. **GPU Before Surface**: The Vulkan Device RHI and Renderer pipelines must be waited on (`vkDeviceWaitIdle()`) and fully destroyed before the OS Window (Vulkan Surface) is deleted. Deleting the window while pipelines are active triggers device lost assertions.
