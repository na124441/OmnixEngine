# Global Variables & Singletons Audit

This document inventories all mutable globals, static variables, and singleton access patterns across the engine, analyzing their architectural risk and prescribing a future migration disposition.

---

## 🌍 Global & Singleton Inventory

| Subsystem / Location | Variable / Pattern | Classification | Risk Level | Proposed Disposition | Migration Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Core (Application.cpp)** | `std::queue<std::string> g_InputQueue` | `TEMPORARY_HACK` | **HIGH** | `RESOLVED_IN_RUNTIME` | Thread and CLI queue are managed safely inside `EngineRuntime` (deprecating old application code). |
| **Core (Application.cpp)** | `std::mutex g_InputMutex` | `TEMPORARY_HACK` | **HIGH** | `RESOLVED_IN_RUNTIME` | Managed inside `EngineRuntime` alongside inputs. |
| **Core (Application.cpp)** | `std::atomic<bool> g_InputThreadRunning` | `TEMPORARY_HACK` | **HIGH** | `RESOLVED_IN_RUNTIME` | Managed thread lifecycle inside `EngineRuntime` prevents thread leaks. |
| **ECS (Serializer/ECS/ECS.h)** | `static ECS* g_Instance;` | `ARCHITECTURAL_RISK` | **HIGH** | `RESOLVED_INJECTED` | Removed singleton instance. Constructor publicized. |
| **ECS (Serializer/ECS/SchemaRegistry.h)** | `static ComponentSchemaRegistry* g_Instance;` | `ARCHITECTURAL_RISK` | **HIGH** | `RESOLVED_INJECTED` | Removed singleton instance and constructor made public. Owned by `EngineRuntime`. |
| **Scene (ComponentFactory.h)** | `static ComponentFactory& GetInstance();` | `TEMPORARY_HACK` | **MEDIUM** | `RESOLVED` | Factory is static-only helper; `GetInstance` was unused and removed. |
| **Core (profiler.h)** | `inline Profiler g_Profiler;` | `VALID_GLOBAL` | **LOW** | `KEEP_GLOBAL` | Profilers have a valid need for global instrumentation. Safe to keep. |

---

## ⚠️ Core Risk Analysis

### 1. Static Initialization Order Fiasco (SIOF)
* **Risk**: High-risk static allocations like `ComponentSchemaRegistry* g_Instance` or `ECS* g_Instance` are constructed lazily, but their destructors are called in undefined order at termination. This can trigger access violations if one static object attempts to reference another during shutdown.

### 2. Thread-Safety Concerns with Stdin/CLI
* **Risk**: The detached thread in `Application.cpp` runs an infinite loop reading `std::cin`. Because it is detached, it is never joined on shutdown. The atomic flag `g_InputThreadRunning` is set to false, but the thread remains blocked on `std::getline(std::cin, line)` until the OS kills the process. This can lead to resource leaks.

### 3. Duplicate ECS State Singletons
* **Risk**: Having `ECS` as a singleton in the Serializer folder while `Coordinator` is a stack/heap allocated object in `Application.cpp` creates severe divergence. A systems programmer might call `ECS::GetInstance()` expecting the gameplay world, only to access a separate test database.
