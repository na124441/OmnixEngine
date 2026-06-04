# Architecture Violations & Prototype Systems Catalog

This document details identified breaches of clean software architecture boundaries, prototype shortcuts, and structural mismatches within **Omnix Studio Engine v0.1**.

---

## 🚨 Critical Architectural Violations

### 1. Diverged ECS & Component Implementations
* **Violation**:
  * There are two completely independent Entity Component Systems in the codebase:
    1. **Gameplay ECS**: Located in `ECS/` (implemented via `Coordinator`, `ComponentManager`, `EntityManager`, etc.).
    2. **Serializer ECS**: Located in `Serializer/ECS/` (implemented via `ECS` and `ComponentPool<T>`).
  * The components are similarly diverged:
    * Gameplay components (`ECS/ECSComponents.h`) use structs like `TransformComponent` (with `Vector3 position, Quaternion rotation, Vector3 scale`).
    * Serializer components (`Components/`) use structs like `Transform` (with `float x, y, z, rotationX, rotationY, rotationZ`).
* **Consequence**:
  * The snapshot serialization module is currently **completely incapable** of saving or restoring the live gameplay world, as it is written for a completely different and duplicate ECS architecture.

### 2. Detached CLI Input Thread Leak
* **Violation**:
  * In `Core/Application.cpp`, `EngineMain` spawns an input polling thread via `std::thread inputThread(InputThreadFunc)` and detaches it.
  * The thread runs an infinite loop calling `std::getline(std::cin, line)`.
* **Consequence**:
  * When the engine shuts down, setting `g_InputThreadRunning = false` has no effect because the detached thread is blocked synchronously on standard input. The thread leaks until the OS forcefully kills the process.

### 3. Dual Transform Sources of Truth
* **Violation**:
  * Both the `Scene` system (`Transform.h` / `SceneNode`) and the gameplay ECS (`TransformComponent` in `ECSComponents.h`) maintain transform/matrix state.
* **Consequence**:
  * Updates to entity positions can diverge between the scene hierarchy and the physics systems, resulting in jittery physics or desynchronized rendering.

---

## 🧪 Temporary Prototype Systems & Hacks

### 1. Hardcoded Local Absolute File Paths
* **Prototype Hack**:
  * Inside `RenderingEngine/Runtime/engine/EngineLoop.h`:
    `#define CUSTOM_MODEL_PATH "E:/Omnix_Studio_v0.1/Engine/RenderingEngine/FinalBaseMesh.obj"`
* **Consequence**:
  * Any attempt to initialize the renderer on a system without an `E:/` drive or this specific path will fail to load the model asset, leading to crashes or empty screens.

### 2. Missing Files in CMake Build Target
* **Prototype Hack**:
  * `CMakeLists.txt` defines:
    `add_executable(serialization_test Test/SerializationTest.cpp)`
* **Consequence**:
  * The directory `Test/` and the file `SerializationTest.cpp` do not exist on disk, meaning the build system is configured with broken references.

### 3. Space in Filename
* **Prototype Hack**:
  * The file `Serializer/Serialization/Normal/NormalSerializer .h` contains a literal space character inside its filename.
* **Consequence**:
  * Can lead to include errors or build failures depending on shell parsing and compiler strictness.

### 4. Skeletal/Empty Subsystems
* **Prototype Hack**:
  * Subsystems under `Systems/` (such as `Systems/Scheduler/SystemScheduler.h`, `Systems/Scheduler/SystemGraph.h`) are 0-byte empty headers.
* **Consequence**:
  * The Scheduler DAG system on paper does not exist in code; execution scheduling is currently hardcoded and sequential inside the game loop in `Application.cpp`.

### 5. Mock AssetCache Class
* **Prototype Hack**:
  * The class `AssetCache` in `RenderingEngine/Runtime/Resources/AssetCache.h` has stub load templates that immediately return a default empty handle:
    `template <typename T> eng::core::Handle<T> Load(const std::string& path) { return eng::core::Handle<T>(); }`
* **Consequence**:
  * There is no actual runtime asset tracking or database backend in the codebase yet.
