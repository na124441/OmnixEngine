# 🔍 Omnix Engine v0.4 Verified Feature Audit

This document presents a strictly verified feature audit of the **Omnix Studio Engine v0.4** codebase. Each feature is audited against the actual source files in the repository and categorized into one of five statuses:
* **IMPLEMENTED**: Code exists, compiles successfully, and is actively integrated and used.
* **PARTIAL**: Code exists but is incomplete or not connected to the main loops.
* **STUB**: Interface or placeholder code exists, but contains no actual behavior.
* **PLANNED**: Mentioned in comments, logs, or planning roadmaps but has no code.
* **MISSING**: Completely absent from the codebase.

---

## 1. Core Runtime

### 1.1 Main Entry Point
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [main.cpp](file:///d:/OmnixEngine/main.cpp)
  * [Core/Application.cpp](file:///d:/OmnixEngine/Core/Application.cpp)
* **Important Classes/Functions**:
  * `int main(int argc, char* argv[])`
  * `eng::runtime::EngineRuntime::Initialize(argc, argv)`
* **Usable for Making Levels**: Yes. Launches the engine runtime.
* **Minimal Example Usage**:
  ```cpp
  // Bootstrap the application in main.cpp
  int main(int argc, char* argv[]) {
      Logger::Init("Omnix.log", LogLevel::Trace);
      {
          eng::runtime::EngineRuntime runtime;
          if (runtime.Initialize(argc, argv)) {
              runtime.Run();
              runtime.Shutdown();
          }
      }
      Logger::Shutdown();
      return 0;
  }
  ```
* **What is Missing for Production**: None. Fully functional entry point.

### 1.2 Application Loop
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L342-L603)
* **Important Classes/Functions**:
  * `void eng::runtime::EngineRuntime::Run()`
* **Usable for Making Levels**: Yes. Drives system execution and frame ticks.
* **Minimal Example Usage**:
  ```cpp
  // Main runtime execution called after successful initialization
  runtime.Run();
  ```
* **What is Missing for Production**: Fully thread-safe sub-stages for parallel jobs. Currently runs mostly sequentially on the main thread.

### 1.3 Delta Time
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L357-L381)
* **Important Classes/Functions**:
  * `eng::runtime::EngineRuntime::Run()`: Computes frame duration via `std::chrono::high_resolution_clock`.
  * `m_Timing.deltaTime` / `m_Timing.frameTime`
* **Usable for Making Levels**: Yes. Scaled update systems use computed delta times.
* **Minimal Example Usage**:
  ```cpp
  // Frame loop timing calculation
  auto frameStart = Clock::now();
  double dt = std::chrono::duration<double>(frameStart - lastFrameTimePoint).count();
  lastFrameTimePoint = frameStart;
  if (dt > 0.1) dt = 0.1; // "Spiral of death" cap
  ```
* **What is Missing for Production**: Integrating delta time smoothing (moving averages) to prevent physics micro-stutters during FPS fluctuations.

### 1.4 Fixed Timestep
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L471-L490)
  * [Physics/Private/PhysicsWorld.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsWorld.cpp)
* **Important Classes/Functions**:
  * `void eng::physics::PhysicsWorld::FixedUpdate(float dt)`
  * `int eng::physics::PhysicsWorld::GetStepsThisFrame()`
  * `float eng::physics::PhysicsWorld::GetFixedTimestep()`
* **Usable for Making Levels**: Yes. Synchronizes PhysX simulation steps and updates gameplay systems deterministically.
* **Minimal Example Usage**:
  ```cpp
  m_PhysicsWorld->FixedUpdate(dt);
  int steps = m_PhysicsWorld->GetStepsThisFrame();
  float fixedDt = m_PhysicsWorld->GetFixedTimestep();
  for (int i = 0; i < steps; ++i) {
      playerControllerSys->FixedUpdate(m_PhysicsWorld.get(), coordinator, fixedDt);
      triggerSys->FixedUpdate(m_Context, fixedDt);
  }
  ```
* **What is Missing for Production**: Interpolation support for visual transforms to render smooth motion when framerate differs from physics update rates.

### 1.5 Game State / Scene State
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Public/Gameplay/GameState.h](file:///d:/OmnixEngine/Runtime/Public/Gameplay/GameState.h)
  * [Runtime/Public/RuntimeState.h](file:///d:/OmnixEngine/Runtime/Public/RuntimeState.h)
* **Important Classes/Functions**:
  * `struct eng::runtime::GameState`
  * `enum class eng::runtime::RuntimeState`
  * `enum class eng::runtime::GameSessionState`
* **Usable for Making Levels**: Yes. State structures track active configurations.
* **Minimal Example Usage**:
  ```cpp
  eng::runtime::GameState state;
  state.ActiveSceneName = "Assets/Scenes/test_room.json";
  state.SessionState = eng::runtime::GameSessionState::Active;
  state.Reset();
  ```
* **What is Missing for Production**: Full replication/sync hooks if networking is added in the future.

### 1.6 Startup and Shutdown Order
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L188-L337) (Initialize)
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L605-L714) (Shutdown)
* **Important Classes/Functions**:
  * `bool eng::runtime::EngineRuntime::Initialize(int argc, char* argv[])`
  * `void eng::runtime::EngineRuntime::Shutdown()`
* **Usable for Making Levels**: Yes. Guarantees error-free boot-ups and memory leaks checks on exit.
* **Minimal Example Usage**:
  ```cpp
  EngineRuntime runtime;
  if (runtime.Initialize(argc, argv)) {
      runtime.Run();
      runtime.Shutdown();
  }
  ```
* **What is Missing for Production**: None. Validation logs confirm a zero-leak deallocation sequence.

---

## 2. ECS

### 2.1 Entity Creation
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/Coordinator.h](file:///d:/OmnixEngine/ECS/Coordinator.h#L36-L39)
  * [ECS/EntityManager.h](file:///d:/OmnixEngine/ECS/EntityManager.h)
  * [ECS/EntityManager.cpp](file:///d:/OmnixEngine/ECS/EntityManager.cpp)
* **Important Classes/Functions**:
  * `Entity Coordinator::CreateEntity()`
  * `Entity EntityManager::CreateEntity()`
* **Usable for Making Levels**: Yes. Spawns entities to assign spatial or logic components.
* **Minimal Example Usage**:
  ```cpp
  Entity myEntity = coordinator.CreateEntity();
  ```
* **What is Missing for Production**: Thread-safe entity creation commands (Entity Command Buffers) to spawn entities from within worker threads.

### 2.2 Entity Destruction
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/Coordinator.h](file:///d:/OmnixEngine/ECS/Coordinator.h#L40-L44)
  * [ECS/EntityManager.cpp](file:///d:/OmnixEngine/ECS/EntityManager.cpp)
* **Important Classes/Functions**:
  * `void Coordinator::DestroyEntity(Entity entity)`
  * `void EntityManager::DestroyEntity(Entity entity)`
* **Usable for Making Levels**: Yes. Cleans up entities dynamically during runtime.
* **Minimal Example Usage**:
  ```cpp
  coordinator.DestroyEntity(myEntity);
  ```
* **What is Missing for Production**: Deferred entity destruction queuing to safely request destructions inside component iteration loops.

### 2.3 Component Registration
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/Coordinator.h](file:///d:/OmnixEngine/ECS/Coordinator.h#L47-L50)
  * [ECS/ComponentManager.h](file:///d:/OmnixEngine/ECS/ComponentManager.h)
* **Important Classes/Functions**:
  * `template<typename T> void Coordinator::RegisterComponent()`
  * `template<typename T> void ComponentManager::RegisterComponent()`
* **Usable for Making Levels**: Yes. Mapped component types are initialized at boot.
* **Minimal Example Usage**:
  ```cpp
  coordinator.RegisterComponent<TransformComponent>();
  ```
* **What is Missing for Production**: None. Components are successfully mapped to a unique compile-time bit representation.

### 2.4 Component Add/Remove
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/Coordinator.h](file:///d:/OmnixEngine/ECS/Coordinator.h#L52-L79)
* **Important Classes/Functions**:
  * `template<typename T> void Coordinator::AddComponent(Entity entity, T component)`
  * `template<typename T> void Coordinator::RemoveComponent(Entity entity)`
* **Usable for Making Levels**: Yes. Allows modifying component attachments.
* **Minimal Example Usage**:
  ```cpp
  TransformComponent trans;
  trans.position = Vector3(10.0f, 0.0f, 5.0f);
  coordinator.AddComponent(myEntity, trans);
  coordinator.RemoveComponent<TransformComponent>(myEntity);
  ```
* **What is Missing for Production**: None. Operations correctly update entity signatures and alert systems.

### 2.5 System Registration
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/Coordinator.h](file:///d:/OmnixEngine/ECS/Coordinator.h#L133-L146)
  * [ECS/SystemManager.h](file:///d:/OmnixEngine/ECS/SystemManager.h)
* **Important Classes/Functions**:
  * `template<typename T> std::shared_ptr<T> Coordinator::RegisterSystem()`
  * `template<typename T> void Coordinator::SetSystemSignature(Signature signature)`
* **Usable for Making Levels**: Yes. Hooks system loops to specific component requirements.
* **Minimal Example Usage**:
  ```cpp
  auto renderSys = coordinator.RegisterSystem<RenderSystem>();
  Signature signature;
  signature.set(coordinator.GetComponentType<TransformComponent>());
  signature.set(coordinator.GetComponentType<RenderableMeshComponent>());
  coordinator.SetSystemSignature<RenderSystem>(signature);
  ```
* **What is Missing for Production**: None. Systems are correctly updated as matching entities change.

### 2.6 System Update Order
* **Status**: `PARTIAL`
* **Evidence**:
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L427-L450)
  * [Systems/Scheduler/SystemScheduler.cpp](file:///d:/OmnixEngine/Systems/Scheduler/SystemScheduler.cpp)
* **Important Classes/Functions**:
  * `void SystemScheduler::RunPending()`
  * `EngineRuntime::Run()` update staging.
* **Usable for Making Levels**: Yes. Runs core updates sequentially.
* **Minimal Example Usage**:
  ```cpp
  // Executed inside loop stages
  m_Scheduler->RunPending();
  ```
* **What is Missing for Production**: The engine scheduler contains a DAG framework, but main update stages (physics, player controller, triggers, interaction) are hardcoded inside [EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp). All systems must be integrated into the DAG scheduler to support automated thread parallelization.

### 2.7 Transform Component
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/ECSComponents.h](file:///d:/OmnixEngine/ECS/ECSComponents.h#L89)
* **Important Classes/Functions**:
  * `struct TransformComponent`
* **Usable for Making Levels**: Yes. Holds coordinates, scale, and rotations.
* **Minimal Example Usage**:
  ```cpp
  TransformComponent trans;
  trans.position = Vector3(0.0f, 5.0f, -2.0f);
  trans.scale = Vector3(1.0f, 1.0f, 1.0f);
  ```
* **What is Missing for Production**: Matrix caching. Currently recalculates coordinate projections on queries.

### 2.8 Render Component
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/ECSComponents.h](file:///d:/OmnixEngine/ECS/ECSComponents.h#L91)
* **Important Classes/Functions**:
  * `struct RenderableMeshComponent`
  * `struct MaterialComponent`
* **Usable for Making Levels**: Yes. Stores references to mesh files and texture profiles.
* **Minimal Example Usage**:
  ```cpp
  RenderableMeshComponent renderMesh;
  renderMesh.meshHandle = 983492837492;
  renderMesh.castShadows = true;
  ```
* **What is Missing for Production**: Skeleton and skinning bindings for animated rendering.

### 2.9 Physics Component
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/ECSComponents.h](file:///d:/OmnixEngine/ECS/ECSComponents.h#L90)
* **Important Classes/Functions**:
  * `struct RigidBodyComponent`
  * `struct StaticBodyComponent`
  * `struct BoxColliderComponent` / `SphereColliderComponent`
* **Usable for Making Levels**: Yes. Simulates physical responses.
* **Minimal Example Usage**:
  ```cpp
  RigidBodyComponent rb;
  rb.mass = 50.0f;
  rb.useGravity = true;
  ```
* **What is Missing for Production**: Support for mesh colliders (concave/convex hulls) parsed from 3D models.

### 2.10 Gameplay Components
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/Coordinator.h](file:///d:/OmnixEngine/ECS/Coordinator.h#L104-L121)
* **Important Classes/Functions**:
  * `struct PlayerStartComponent`, `struct CharacterControllerComponent`, `struct TriggerComponent`, `struct ObjectiveComponent`, `struct CheckpointComponent`, `struct DoorComponent`, `struct SimpleStateComponent`
* **Usable for Making Levels**: Yes. Defines game mechanics on entities.
* **Minimal Example Usage**:
  ```cpp
  ObjectiveComponent objComp;
  objComp.ObjectiveID = "OBJ_MainQuest";
  objComp.CompletionMode = ObjectiveCompletionMode::TriggerEnter;
  ```
* **What is Missing for Production**: Fully unified component scripting/callbacks (currently handled by C++ gameplay systems).

---

## 3. Scene System

### 3.1 Scene Object/Class
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneObject.h](file:///d:/OmnixEngine/Scene/SceneObject.h)
  * [Scene/SceneObject.cpp](file:///d:/OmnixEngine/Scene/SceneObject.cpp)
* **Important Classes/Functions**:
  * `class SceneObject`
* **Usable for Making Levels**: Yes. Organizes parent-child entity trees.
* **Minimal Example Usage**:
  ```cpp
  auto rootNode = std::make_shared<SceneObject>("RootLabNode");
  auto doorNode = std::make_shared<SceneObject>("SecDoor");
  doorNode->SetParent(rootNode);
  ```
* **What is Missing for Production**: Multi-thread safe hierarchy changes.

### 3.2 Scene Manager
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneManager.h](file:///d:/OmnixEngine/Scene/SceneManager.h)
  * [Scene/SceneManager.cpp](file:///d:/OmnixEngine/Scene/SceneManager.cpp)
* **Important Classes/Functions**:
  * `class SceneManager`
* **Usable for Making Levels**: Yes. Drives active scene transactions.
* **Minimal Example Usage**:
  ```cpp
  m_Scenes->LoadScene("Assets/Scenes/test_room.json");
  ```
* **What is Missing for Production**: Pre-allocated scene transition memory pools to avoid runtime dynamic allocations.

### 3.3 Scene Loading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneLoader.cpp](file:///d:/OmnixEngine/Scene/SceneLoader.cpp)
* **Important Classes/Functions**:
  * `static Scene* SceneLoader::LoadFromFile(const std::string& filePath)`
* **Usable for Making Levels**: Yes. Parses and instantiates json level layouts.
* **Minimal Example Usage**:
  ```cpp
  Scene* loadedLevel = SceneLoader::LoadFromFile("Assets/Scenes/test_room.json");
  ```
* **What is Missing for Production**: Asynchronous multi-threaded loading (disk read and object construction currently block the main render frame).

### 3.4 Scene Unloading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneManager.cpp](file:///d:/OmnixEngine/Scene/SceneManager.cpp#L209-L216)
* **Important Classes/Functions**:
  * `void SceneManager::UnloadActiveScene()`
* **Usable for Making Levels**: Yes. Frees entities, lookups, and meshes.
* **Minimal Example Usage**:
  ```cpp
  // Handled automatically during scene switches
  sceneManager->LoadScene("NextLevel.json");
  ```
* **What is Missing for Production**: Deferred garbage collection of assets to keep shared resources (textures/materials) in cache if needed by subsequent levels.

### 3.5 Scene Switching
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneManager.cpp](file:///d:/OmnixEngine/Scene/SceneManager.cpp#L100-L101)
* **Important Classes/Functions**:
  * `void SceneManager::SwitchScene()`
* **Usable for Making Levels**: Yes. Performs the unload-load transition safely.
* **Minimal Example Usage**:
  ```cpp
  sceneManager->LoadScene("Assets/Scenes/lab.json");
  // Update loop processes switch automatically
  ```
* **What is Missing for Production**: Visual transition fades (fade to black) to mask loading pop-ins.

### 3.6 Additive Scene Loading
* **Status**: `MISSING`
* **Evidence**:
  * Verified absence in `SceneManager.cpp` and `SceneLoader.cpp`. The loading pipeline always destroys and unloads the active scene before starting a load.
* **Important Classes/Functions**: None.
* **Usable for Making Levels**: No.
* **Minimal Example Usage**: N/A
* **What is Missing for Production**: Add an `additive` parameter flag to `LoadScene` that skips `UnloadActiveScene()` and appends incoming scene objects directly to the existing hierarchy and ECS Coordinator.

### 3.7 Loading Screen State
* **Status**: `STUB`
* **Evidence**:
  * [Scene/SceneManager.h](file:///d:/OmnixEngine/Scene/SceneManager.h#L43-L47) (Defines `TransitionState::Loading`)
  * [Scene/SceneManager.cpp](file:///d:/OmnixEngine/Scene/SceneManager.cpp)
* **Important Classes/Functions**:
  * `SceneManager::TransitionState::Loading`
* **Usable for Making Levels**: No. No visual loader screen is rendered.
* **Minimal Example Usage**: N/A
* **What is Missing for Production**: Link the loading transition state directly to the graphics engine to render a visual layout card while the loader thread is active.

### 3.8 Scene Save Support
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp)
* **Important Classes/Functions**:
  * `static bool SceneSerializer::SaveScene(Scene* scene, const std::string& filePath)`
* **Usable for Making Levels**: Yes. Saves the editor's modifications to disk.
* **Minimal Example Usage**:
  ```cpp
  SceneSerializer::SaveScene(activeScene, "Assets/Scenes/lab_edit.json");
  ```
* **What is Missing for Production**: Automatic save file backups to prevent file corruption if a write is interrupted.

---

## 4. Level File Format

### 4.1 `.omnixscene` Support
* **Status**: `PARTIAL`
* **Evidence**:
  * [Runtime/Public/OmnixSceneFormat.h](file:///d:/OmnixEngine/Runtime/Public/OmnixSceneFormat.h)
  * [Runtime/Private/FormatTests.cpp](file:///d:/OmnixEngine/Runtime/Private/FormatTests.cpp#L296)
* **Important Classes/Functions**:
  * `struct OmnixScene`
  * `bool SerializeScene(const OmnixScene& scene, const std::string& filepath)`
* **Usable for Making Levels**: No. Binary `.omnixscene` is not hooked up to the runtime level loader.
* **Minimal Example Usage**:
  ```cpp
  // Used only in validation tests currently
  OmnixScene binaryScene;
  SerializeScene(binaryScene, "test.omnixscene");
  ```
* **What is Missing for Production**: Implement binary scene serialization and loading paths within the main `SceneManager` interface.

### 4.2 JSON Scene Support
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneLoader.cpp](file:///d:/OmnixEngine/Scene/SceneLoader.cpp)
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp)
* **Important Classes/Functions**:
  * `SceneLoader::LoadFromFile`
  * `SceneSerializer::SaveScene`
* **Usable for Making Levels**: Yes. Default scene file format.
* **Minimal Example Usage**:
  ```json
  // Scene representation in JSON
  {
    "SceneName": "DemoScene",
    "Entities": [ { "EntityID": 1, "Name": "Spawn" } ]
  }
  ```
* **What is Missing for Production**: Full JSON format validation using a schema definition.

### 4.3 Binary Scene Support
* **Status**: `PARTIAL`
* **Evidence**:
  * Same as `.omnixscene` (uses custom delta/snapshot serialization).
* **Important Classes/Functions**:
  * `bool DeserializeScene(OmnixScene& scene, const std::string& filepath)`
* **Usable for Making Levels**: No. Not integrated with main loaders.
* **Minimal Example Usage**:
  ```cpp
  OmnixScene binaryScene;
  DeserializeScene(binaryScene, "Level.omnixscene");
  ```
* **What is Missing for Production**: Connecting the deserializer to instantiating live ECS entities.

### 4.4 Entity Serialization
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp#L429)
  * [Runtime/Public/OmnixSceneFormat.h](file:///d:/OmnixEngine/Runtime/Public/OmnixSceneFormat.h#L11-L23)
* **Important Classes/Functions**:
  * `SceneSerializer::SerializeSceneObject()`
  * `struct SceneEntityRecord`
* **Usable for Making Levels**: Yes. Saves entities to files.
* **Minimal Example Usage**: Handled internally during scene save operations.
* **What is Missing for Production**: None.

### 4.5 Component Serialization
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp#L27)
  * [Runtime/Public/OmnixSceneFormat.h](file:///d:/OmnixEngine/Runtime/Public/OmnixSceneFormat.h#L68-L77)
* **Important Classes/Functions**:
  * `SceneSerializer::SerializeComponents()`
  * `struct SceneComponentDataBlock`
* **Usable for Making Levels**: Yes. Saves components attached to entities.
* **Minimal Example Usage**: Handled internally during scene save operations.
* **What is Missing for Production**: Component schema evolution rules to load legacy level files when component structures change.

### 4.6 Transform Serialization
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp#L26)
* **Important Classes/Functions**:
  * `SceneSerializer::SerializeTransform()`
* **Usable for Making Levels**: Yes. Position, rotation, and scale values are written to JSON.
* **Minimal Example Usage**: Handled internally during scene save operations.
* **What is Missing for Production**: None.

### 4.7 Asset Reference Serialization
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp)
  * [Runtime/Public/OmnixSceneFormat.h](file:///d:/OmnixEngine/Runtime/Public/OmnixSceneFormat.h#L58-L66)
* **Important Classes/Functions**:
  * `struct SceneAssetReferenceEntry`
* **Usable for Making Levels**: Yes. Resolves mesh/material paths using UUID handles.
* **Minimal Example Usage**: Handled internally during scene save operations.
* **What is Missing for Production**: None.

### 4.8 Prefab Serialization
* **Status**: `PARTIAL`
* **Evidence**:
  * [Scene/SceneSerializer.cpp](file:///d:/OmnixEngine/Scene/SceneSerializer.cpp#L20) (SavePrefab is a stub returning false)
  * [Scene/PrefabRegistry.cpp](file:///d:/OmnixEngine/Scene/PrefabRegistry.cpp#L145) (disk loader prints warnings)
* **Important Classes/Functions**:
  * `SceneSerializer::SavePrefab`
  * `PrefabRegistry::LoadPrefabFromFile`
* **Usable for Making Levels**: No. STANDALONE prefabs cannot be saved to or loaded from disk yet.
* **Minimal Example Usage**: N/A
* **What is Missing for Production**: Fully implementing `SavePrefab` and integrating the prefab loader into the editor toolchain.

---

## 5. Asset Pipeline

### 5.1 Asset Folder Structure
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * Directory contents of `d:\OmnixEngine\Assets`
  * [README.md](file:///d:/OmnixEngine/README.md#L121)
* **Important Classes/Functions**: N/A
* **Usable for Making Levels**: Yes. Standard directory layouts.
* **Minimal Example Usage**: Place meshes in `Assets/Meshes/`, audio in `Assets/Audio/`, etc.
* **What is Missing for Production**: None.

### 5.2 Mesh Loading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/MeshImporter.cpp](file:///d:/OmnixEngine/Runtime/Private/MeshImporter.cpp)
  * [RenderingEngine/Renderer/scene/ModelLoader.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/scene/ModelLoader.cpp)
* **Important Classes/Functions**:
  * `bool DeserializeMesh(OmnixMesh& mesh, const std::string& filepath)`
  * `static std::vector<Mesh*> ModelLoader::LoadModel(...)`
* **Usable for Making Levels**: Yes. Standard models render.
* **Minimal Example Usage**:
  ```cpp
  OmnixMesh myMesh;
  DeserializeMesh(myMesh, "Assets/Meshes/cube.omnixmesh");
  ```
* **What is Missing for Production**: None.

### 5.3 Texture Loading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/TextureImporter.cpp](file:///d:/OmnixEngine/Runtime/Private/TextureImporter.cpp)
  * [RenderingEngine/Renderer/scene/Texture.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/scene/Texture.cpp)
* **Important Classes/Functions**:
  * `Texture::Load(const std::string& path)`
  * Uses `stb_image` for loading PNG/JPG files.
* **Usable for Making Levels**: Yes. Standard textures load.
* **Minimal Example Usage**:
  ```cpp
  Texture myTexture;
  myTexture.Load("Assets/Textures/brick_albedo.png");
  ```
* **What is Missing for Production**: Block compression support (BC7/ASTC) during texture imports to save GPU memory.

### 5.4 Material Loading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/SceneLoader.cpp](file:///d:/OmnixEngine/Scene/SceneLoader.cpp)
  * [Runtime/Private/AssetManager.cpp](file:///d:/OmnixEngine/Runtime/Private/AssetManager.cpp)
* **Important Classes/Functions**:
  * `Material* AssetManager::LoadMaterial(AssetHandle handle)`
* **Usable for Making Levels**: Yes. Parses and loads `.omnixmat` files.
* **Minimal Example Usage**: Done automatically by `SceneLoader` when entities have a `MaterialComponent`.
* **What is Missing for Production**: Support for custom shading graphs (currently limited to standard PBR material parameters).

### 5.5 Audio Loading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Audio/AudioSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Audio/AudioSystem.cpp)
* **Important Classes/Functions**:
  * `void AudioSystem::PreloadClip(const std::string& path)`
* **Usable for Making Levels**: Yes. WAV and OGG sound clips load.
* **Minimal Example Usage**:
  ```cpp
  // Managed by AudioSystem on requests
  m_AudioSystem->PlayOneShot("Assets/Audio/click.wav");
  ```
* **What is Missing for Production**: None.

### 5.6 UUID / Handle System
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Public/AssetHandle.h](file:///d:/OmnixEngine/Runtime/Public/AssetHandle.h)
  * [Runtime/Private/AssetManager.cpp](file:///d:/OmnixEngine/Runtime/Private/AssetManager.cpp)
* **Important Classes/Functions**:
  * `struct AssetHandle` (wraps `uint64_t`)
  * `static uint64_t GenerateAssetUUID(const std::string& canonicalPath)`: Computes FNV-1a hash.
* **Usable for Making Levels**: Yes. Assets are referenced by stable UUIDs instead of path strings.
* **Minimal Example Usage**:
  ```cpp
  AssetHandle handle = GenerateAssetUUID("Assets/Meshes/cube.obj");
  ```
* **What is Missing for Production**: Collision resolution for FNV-1a hashes (extremely rare but possible).

### 5.7 Asset Registry
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Public/AssetRegistry.h](file:///d:/OmnixEngine/Runtime/Public/AssetRegistry.h)
  * [Runtime/Private/AssetRegistry.cpp](file:///d:/OmnixEngine/Runtime/Private/AssetRegistry.cpp)
  * [AssetRegistry.json](file:///d:/OmnixEngine/AssetRegistry.json)
* **Important Classes/Functions**:
  * `class AssetRegistry`
  * `void LoadRegistry(const std::string& filePath)`
* **Usable for Making Levels**: Yes. Registry tracks asset locations and UUID mappings.
* **Minimal Example Usage**:
  ```cpp
  AssetRegistry registry;
  registry.LoadRegistry("AssetRegistry.json");
  ```
* **What is Missing for Production**: Asset database optimization for projects with tens of thousands of assets (currently parses standard JSON).

### 5.8 Asset Import
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/AssetImportService.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/AssetImportService.cpp)
* **Important Classes/Functions**:
  * `AssetHandle AssetImportService::ImportAsset(const std::string& sourcePath, AssetType type)`
* **Usable for Making Levels**: Yes. Editor handles file imports and updates registries.
* **Minimal Example Usage**:
  ```cpp
  AssetImportService::ImportAsset("source_mesh.obj", AssetType::Mesh);
  ```
* **What is Missing for Production**: None.

### 5.9 Asset Compiler
* **Status**: `STUB`
* **Evidence**:
  * [FuturePlan.md](file:///d:/OmnixEngine/FuturePlan.md#L731-L735)
  * [Docs/omnix_v0.3_architectural_report.md](file:///d:/OmnixEngine/Docs/omnix_v0.3_architectural_report.md#L135-L144)
* **Important Classes/Functions**:
  * `omnix-cli` mentioned as planned headless compiler target in `FuturePlan.md` and `CMakeLists.txt`, but no `omnix-cli` executable exists or is built.
* **Usable for Making Levels**: No.
* **Minimal Example Usage**: N/A
* **What is Missing for Production**: Extracting mesh and texture compilation code from Editor code and building the standalone CLI target.

### 5.10 Hot Reload
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/HotReloadSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/HotReloadSystem.cpp)
  * [Runtime/Private/FileWatcher.cpp](file:///d:/OmnixEngine/Runtime/Private/FileWatcher.cpp)
* **Important Classes/Functions**:
  * `class HotReloadSystem`
  * `class FileWatcher`
* **Usable for Making Levels**: Yes. Visual modifications reload live.
* **Minimal Example Usage**: Modify a shader or texture file on disk while the game is running, and changes are reflected instantly.
* **What is Missing for Production**: Thread safety when hot reloading a resource that is currently actively bound in a render command list.

---

## 6. Rendering

### 6.1 Rendering Backend
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Vulkan/VulkanInstance.cpp](file:///d:/OmnixEngine/RenderingEngine/Vulkan/VulkanInstance.cpp)
  * [RenderingEngine/Vulkan/VulkanDevice.cpp](file:///d:/OmnixEngine/RenderingEngine/Vulkan/VulkanDevice.cpp)
* **Important Classes/Functions**:
  * `class VulkanInstance`, `class VulkanDevice`, `class VulkanSwapChain`
* **Usable for Making Levels**: Yes. Drives Vulkan graphics pipelines.
* **Minimal Example Usage**: Initialized automatically during boot.
* **What is Missing for Production**: None.

### 6.2 Window Creation
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Platform/window/Window.cpp](file:///d:/OmnixEngine/RenderingEngine/Platform/window/Window.cpp)
* **Important Classes/Functions**:
  * `bool Window::Initialize()`
* **Usable for Making Levels**: Yes. Spawns desktop windows.
* **Minimal Example Usage**: Initialized automatically during boot.
* **What is Missing for Production**: Multi-monitor window support.

### 6.3 Mesh Rendering
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Renderer/SceneRenderer.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/SceneRenderer.cpp)
* **Important Classes/Functions**:
  * `void SceneRenderer::DrawScene()`
  * `void Mesh::Draw(VkCommandBuffer cmd)`
* **Usable for Making Levels**: Yes. Standard meshes render correctly.
* **Minimal Example Usage**: Attach a `RenderableMeshComponent` to an entity.
* **What is Missing for Production**: Dynamic mesh LOD transitions.

### 6.4 Multiple Objects
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Renderer/SceneRenderer.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/SceneRenderer.cpp)
* **Important Classes/Functions**:
  * Iterates over all entities in the `RenderSystem` and records draw commands.
* **Usable for Making Levels**: Yes. Renders complex scenes with multiple mesh nodes.
* **Minimal Example Usage**: Spawn multiple entities with render components.
* **What is Missing for Production**: Occlusion culling to avoid rendering objects blocked by walls.

### 6.5 Independent Transforms
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Renderer/SceneRenderer.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/SceneRenderer.cpp)
* **Important Classes/Functions**:
  * Updates push constants or UBO matrices using the entity's `TransformComponent`.
* **Usable for Making Levels**: Yes. Objects render at their individual locations.
* **Minimal Example Usage**: Move transform positions in the editor.
* **What is Missing for Production**: Transform interpolation on lag frames.

### 6.6 Camera Support
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Scene/Camera.cpp](file:///d:/OmnixEngine/Scene/Camera.cpp)
  * [ECS/CameraSystem.h](file:///d:/OmnixEngine/ECS/CameraSystem.h)
* **Important Classes/Functions**:
  * `class Camera`
  * `class CameraSystem`
* **Usable for Making Levels**: Yes. Controls render viewports.
* **Minimal Example Usage**: Attach a `CameraComponent` to an entity.
* **What is Missing for Production**: Frustum culling (meshes outside the camera view are currently still submitted to the GPU).

### 6.7 Light Support
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/LightCollectionSystem.h](file:///d:/OmnixEngine/ECS/LightCollectionSystem.h)
  * [RenderingEngine/Renderer/SceneRenderer.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/SceneRenderer.cpp#L736)
* **Important Classes/Functions**:
  * `struct PointLightComponent`
  * `struct DirectionalLightComponent`
  * `class LightCollectionSystem`
* **Usable for Making Levels**: Yes. Renders lighting models.
* **Minimal Example Usage**: Attach a `PointLightComponent` to a lamp entity.
* **What is Missing for Production**: Dynamic shadow mapping (lights currently do not cast shadows).

### 6.8 Material Support
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Renderer/scene/Material.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/scene/Material.cpp)
* **Important Classes/Functions**:
  * `class Material`
* **Usable for Making Levels**: Yes. Renders albedo/roughness textures on meshes.
* **Minimal Example Usage**: Assign a material to a render component.
* **What is Missing for Production**: Multi-material blending.

### 6.9 Shader Loading
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [RenderingEngine/Renderer/scene/Shader.cpp](file:///d:/OmnixEngine/RenderingEngine/Renderer/scene/Shader.cpp)
  * [CMakeLists.txt](file:///d:/OmnixEngine/CMakeLists.txt#L195)
* **Important Classes/Functions**:
  * `bool Shader::LoadSPIRV(...)`
* **Usable for Making Levels**: Yes. Reads precompiled shaders.
* **Minimal Example Usage**: Managed internally by the rendering pipeline.
* **What is Missing for Production**: None.

### 6.10 Skybox
* **Status**: `PLANNED`
* **Evidence**:
  * Mentioned in `FuturePlan.md` but has no implementation in the rendering system.
* **Important Classes/Functions**: None.
* **Usable for Making Levels**: No.
* **Minimal Example Usage**: N/A
* **What is Missing for Production**: Implement cubemap texture loading and a skybox render pass.

### 6.11 UI Rendering
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/UI/GameplayHUD.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/UI/GameplayHUD.cpp)
* **Important Classes/Functions**:
  * `void GameplayHUD::Render()`
* **Usable for Making Levels**: Yes. HUD overlays ("Press E to interact", objective cards) render.
* **Minimal Example Usage**: HUD rendering is driven by gameplay events.
* **What is Missing for Production**: Custom HUD canvas editing (currently hardcoded ImGui layouts).

### 6.12 Debug Rendering
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsDebugDraw.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsDebugDraw.cpp)
* **Important Classes/Functions**:
  * `PhysicsDebugDraw::DrawCollider`
* **Usable for Making Levels**: Yes. Renders collision bounds in the editor viewport.
* **Minimal Example Usage**: Checked internally when play/stop simulations are toggled.
* **What is Missing for Production**: None.

---

## 7. Physics / Collision

### 7.1 Physics Backend
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsWorld.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsWorld.cpp)
* **Important Classes/Functions**:
  * `class eng::physics::PhysicsWorld`
* **Usable for Making Levels**: Yes. Steps PhysX scene simulation.
* **Minimal Example Usage**: Managed by `EngineRuntime`.
* **What is Missing for Production**: None.

### 7.2 Rigid Body
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsWorld.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsWorld.cpp)
  * [ECS/PhysicsSystem.h](file:///d:/OmnixEngine/ECS/PhysicsSystem.h)
* **Important Classes/Functions**:
  * `struct RigidBodyComponent`
* **Usable for Making Levels**: Yes. Dynamic dynamic rigid actors.
* **Minimal Example Usage**: Attach a `RigidBodyComponent` to a mesh.
* **What is Missing for Production**: Dynamic force manipulation APIs (e.g. `AddForce`, `AddTorque`) exposed to components.

### 7.3 Static Collider
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsWorld.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsWorld.cpp#L110)
* **Important Classes/Functions**:
  * `void PhysicsWorld::RegisterStaticColliders(...)`
* **Usable for Making Levels**: Yes. Spawns colliders that block dynamic movement.
* **Minimal Example Usage**: Attach a `StaticBodyComponent` and `BoxColliderComponent` to a floor.
* **What is Missing for Production**: None.

### 7.4 Trigger Volume
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/TriggerSystem.h](file:///d:/OmnixEngine/ECS/TriggerSystem.h)
* **Important Classes/Functions**:
  * `struct TriggerComponent`
  * `class TriggerSystem`
* **Usable for Making Levels**: Yes. Fires overlap events.
* **Minimal Example Usage**: Attach a `TriggerComponent` to an entity.
* **What is Missing for Production**: None.

### 7.5 Raycast
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsWorld.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsWorld.cpp#L205)
* **Important Classes/Functions**:
  * `bool PhysicsWorld::Raycast(...)`
* **Usable for Making Levels**: Yes. Returns hit intersection information.
* **Minimal Example Usage**:
  ```cpp
  eng::physics::RaycastHit hit;
  m_PhysicsWorld->Raycast(origin, direction, 100.0f, hit);
  ```
* **What is Missing for Production**: Collision layer filtering (checking specific flags to ignore player/translucent meshes).

### 7.6 Character Controller
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsWorld.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsWorld.cpp)
  * [ECS/PlayerControllerSystem.h](file:///d:/OmnixEngine/ECS/PlayerControllerSystem.h)
* **Important Classes/Functions**:
  * `struct CharacterControllerComponent`
  * `class PlayerControllerSystem`
* **Usable for Making Levels**: Yes. Controls player physical interactions.
* **Minimal Example Usage**: Attach `CharacterControllerComponent` to player.
* **What is Missing for Production**: Slopes/stairs stepping behaviors (currently player gets stuck on small steps).

### 7.7 Collision Events
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/TriggerSystem.h](file:///d:/OmnixEngine/ECS/TriggerSystem.h)
* **Important Classes/Functions**:
  * Fires `TriggerEnter` and `TriggerExit` gameplay events.
* **Usable for Making Levels**: Yes. Triggering overlap zones notifies systems.
* **Minimal Example Usage**: Hook up a door to open when a keycard trigger zone is entered.
* **What is Missing for Production**: Dynamic collision impact velocity checks (e.g. knowing how hard a rigid body hit a wall).

### 7.8 Debug Draw
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Physics/Private/PhysicsDebugDraw.cpp](file:///d:/OmnixEngine/Physics/Private/PhysicsDebugDraw.cpp)
* **Important Classes/Functions**:
  * `PhysicsDebugDraw::DrawCollider`
* **Usable for Making Levels**: Yes. Renders outlines of boxes/spheres.
* **Minimal Example Usage**: Managed internally by the rendering backend.
* **What is Missing for Production**: None.

---

## 8. Gameplay Authoring

### 8.1 Player Spawn
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/GameMode.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/GameMode.cpp)
* **Important Classes/Functions**:
  * `struct PlayerStartComponent`
  * `Entity GameMode::FindPlayerEntity() const`
* **Usable for Making Levels**: Yes. Spawns player at coordinates defined by `PlayerStartComponent`.
* **Minimal Example Usage**: Place an entity in a level carrying a `PlayerStartComponent`.
* **What is Missing for Production**: Multi-spawn point logic for checkpoints or multiplayer.

### 8.2 Player Controller
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [ECS/PlayerControllerSystem.h](file:///d:/OmnixEngine/ECS/PlayerControllerSystem.h)
* **Important Classes/Functions**:
  * `class PlayerControllerSystem`
  * `struct PlayerControllerComponent`
* **Usable for Making Levels**: Yes. Handles mouse-cam looks and WASD character movements.
* **Minimal Example Usage**: Attach `PlayerControllerComponent` to player.
* **What is Missing for Production**: Customizable mouse sensitivity configuration settings.

### 8.3 Interactable Object
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/Systems/InteractionSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/Systems/InteractionSystem.cpp)
* **Important Classes/Functions**:
  * `class InteractionSystem`
  * `struct InteractableComponent`
* **Usable for Making Levels**: Yes. Triggers actions on key press.
* **Minimal Example Usage**: Attach `InteractableComponent` to terminal.
* **What is Missing for Production**: Interactive HUD prompts customizable per object.

### 8.4 Objective System
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/Objectives/ObjectiveSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/Objectives/ObjectiveSystem.cpp)
* **Important Classes/Functions**:
  * `class ObjectiveSystem`
  * `struct ObjectiveComponent`
* **Usable for Making Levels**: Yes. Drives level progress.
* **Minimal Example Usage**:
  ```cpp
  objectiveSystem->StartObjective("OBJ_UnlockLab");
  ```
* **What is Missing for Production**: Hierarchical objectives (sub-tasks).

### 8.5 Checkpoint System
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/CheckpointSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/CheckpointSystem.cpp)
* **Important Classes/Functions**:
  * `class CheckpointSystem`
  * `struct CheckpointComponent`
* **Usable for Making Levels**: Yes. Re-spawns player on death.
* **Minimal Example Usage**: Attach `CheckpointComponent` to an overlap volume.
* **What is Missing for Production**: Visual check chimes/particle effects.

### 8.6 Door / Activatable System
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/StateObjects/ObjectActivationSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/StateObjects/ObjectActivationSystem.cpp)
* **Important Classes/Functions**:
  * `class ObjectActivationSystem`
  * `struct ActivatableComponent`
  * `struct DoorComponent`
* **Usable for Making Levels**: Yes. Links triggers to door opening.
* **Minimal Example Usage**: Hook activation IDs between triggers and doors.
* **What is Missing for Production**: Rotating door behaviors (currently only translates on offset vector).

### 8.7 Audio Source
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Audio/AudioSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Audio/AudioSystem.cpp)
* **Important Classes/Functions**:
  * `struct AudioSourceComponent`
  * `void AudioSystem::PlayEntitySound(...)`
* **Usable for Making Levels**: Yes. Sound triggers at coordinates.
* **Minimal Example Usage**: Attach an `AudioSourceComponent` to an entity.
* **What is Missing for Production**: 3D spatial HRTF audio coordinates calculation (currently master volumes scaling based on distance).

### 8.8 Cutscene Trigger
* **Status**: `PLANNED`
* **Evidence**:
  * Outlined in `FuturePlan.md` but not found in the codebase.
* **Important Classes/Functions**: None.
* **Usable for Making Levels**: No.
* **Minimal Example Usage**: N/A
* **What is Missing for Production**: Camera track interpolation and sequencer framework.

### 8.9 Level Transition
* **Status**: `PARTIAL`
* **Evidence**:
  * [Runtime/Private/Gameplay/GameMode.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/GameMode.cpp#L31)
  * [Scene/SceneManager.cpp](file:///d:/OmnixEngine/Scene/SceneManager.cpp)
* **Important Classes/Functions**:
  * `GameMode::CompleteLevel()`
  * `SceneManager::LoadScene()`
* **Usable for Making Levels**: Yes. Triggers new scene loading.
* **Minimal Example Usage**: Call `CompleteLevel()` on objective completion.
* **What is Missing for Production**: Visual transition effects (loading screens, fading).

### 8.10 Save / Load Gameplay State
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Gameplay/Save/GameplaySaveSystem.cpp](file:///d:/OmnixEngine/Runtime/Private/Gameplay/Save/GameplaySaveSystem.cpp)
* **Important Classes/Functions**:
  * `GameplaySaveSystem::CaptureSnapshot()`, `SaveToFile()`, `LoadFromFile()`
* **Usable for Making Levels**: Yes. Saves player status and levels progress.
* **Minimal Example Usage**:
  ```cpp
  GameplaySaveSnapshot snap = saveSystem->CaptureSnapshot();
  saveSystem->SaveToFile("Saves/save1.sav", snap);
  ```
* **What is Missing for Production**: Asynchronous save/load file writes to avoid freezing the frame.

---

## 9. Editor

### 9.1 Editor Executable / Mode
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L119)
* **Important Classes/Functions**:
  * Launch with `--editor` command-line argument.
* **Usable for Making Levels**: Yes. Opens dockable editor layout.
* **Minimal Example Usage**:
  `./Application.exe --editor`
* **What is Missing for Production**: Standalone editor application target separate from the runtime.

### 9.2 Viewport
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/Panels/ViewportPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/ViewportPanel.cpp)
* **Important Classes/Functions**:
  * `class ViewportPanel`
* **Usable for Making Levels**: Yes. Visualizes offscreen render outputs.
* **Minimal Example Usage**: Toggled automatically in editor mode.
* **What is Missing for Production**: Viewport grid rendering and lighting mode toggles (lit, wireframe, unlit).

### 9.3 Hierarchy Panel
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp)
* **Important Classes/Functions**:
  * `class SceneHierarchyPanel`
* **Usable for Making Levels**: Yes. Displays scene entity trees.
* **Minimal Example Usage**: Select entities to open in inspector.
* **What is Missing for Production**: Drag-and-drop parenting reorganizations in tree nodes.

### 9.4 Inspector Panel
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/Panels/InspectorPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/InspectorPanel.cpp)
* **Important Classes/Functions**:
  * `class InspectorPanel`
* **Usable for Making Levels**: Yes. Displays component values.
* **Minimal Example Usage**: Edit positions or speeds in inspector fields.
* **What is Missing for Production**: Undo/redo history triggers on value changes.

### 9.5 Transform Editing
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/Widgets/TransformWidget.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Widgets/TransformWidget.cpp)
  * [ThirdParty/ImGuizmo/ImGuizmo.cpp](file:///d:/OmnixEngine/ThirdParty/ImGuizmo/ImGuizmo.cpp)
* **Important Classes/Functions**:
  * `TransformWidget::Draw`
  * `ImGuizmo::Manipulate`
* **Usable for Making Levels**: Yes. Manipulate entities with viewport gizmos.
* **Minimal Example Usage**: Press 'W' to translate, 'E' to rotate, 'R' to scale selected meshes.
* **What is Missing for Production**: Snapping settings.

### 9.6 Component Editing
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/Widgets/ComponentWidgets.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Widgets/ComponentWidgets.cpp)
* **Important Classes/Functions**:
  * Component inspector widgets.
* **Usable for Making Levels**: Yes. Edits components directly.
* **Minimal Example Usage**: Modify door travel offsets or trigger sizes in the editor UI.
* **What is Missing for Production**: Dynamic component type additions (currently must compile component enums in C++).

### 9.7 Asset Browser
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/Panels/AssetBrowserPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/AssetBrowserPanel.cpp)
* **Important Classes/Functions**:
  * `class AssetBrowserPanel`
* **Usable for Making Levels**: Yes. Mapped directories display files.
* **Minimal Example Usage**: Double-click assets to import/assign.
* **What is Missing for Production**: Asset search and tag filtering.

### 9.8 Scene Save
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp)
* **Important Classes/Functions**:
  * Menu save triggers `SceneManager::SaveActiveScene`.
* **Usable for Making Levels**: Yes. File -> Save Scene saves JSON configurations.
* **What is Missing for Production**: None.

### 9.9 Scene Load
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp)
* **Important Classes/Functions**:
  * Menu load triggers `SceneManager::LoadScene`.
* **Usable for Making Levels**: Yes. Loads scenes from file selector.
* **What is Missing for Production**: None.

### 9.10 Play Mode
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [Runtime/Private/Editor/EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp)
  * [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp)
* **Important Classes/Functions**:
  * Toggles `EditorSimulationState::Play`. Clones active scene to restore state on stop.
* **Usable for Making Levels**: Yes. Toolbar Play button runs simulation.
* **What is Missing for Production**: Frame pausing and step-by-step frame debugging.

---

## 10. Build and Run

### 10.1 Exact Build Commands
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [README.md](file:///d:/OmnixEngine/README.md)
  * [v0.3_VALIDATION_RESULTS.md](file:///d:/OmnixEngine/v0.3_VALIDATION_RESULTS.md)
  * [build_msvc.bat](file:///d:/OmnixEngine/build_msvc.bat)
* **Usable for Making Levels**: Yes. Builds the target binary.
* **Exact Command**:
  * **Standard CMake**:
    ```powershell
    mkdir build
    cd build
    cmake ..
    cmake --build . --config Release
    ```
  * **Batch File Command**:
    ```powershell
    .\build_msvc.bat
    ```

### 10.2 Exact Run Commands
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * Same files as build commands.
* **Usable for Making Levels**: Yes. Runs applications.
* **Exact Command**:
  * **Running Editor Mode**:
    ```powershell
    .\build_ninja\Application.exe --editor
    ```
  * **Running Game Mode**:
    ```powershell
    .\build_ninja\Application.exe
    ```
  * **Running Diagnostics Tests**:
    ```powershell
    .\build_ninja\Application.exe --test-memory
    ```

### 10.3 Tested Compiler
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * [v0.3_VALIDATION_RESULTS.md](file:///d:/OmnixEngine/v0.3_VALIDATION_RESULTS.md)
* **Usable for Making Levels**: Yes. Compiler outputs executable.
* **Details**: MSVC x64 (tested on VS 2026 Developer Command Prompt v18.7.0-insiders) on Windows.

### 10.4 Known Errors
* **Status**: `PARTIAL`
* **Evidence**:
  * [v0.3_VALIDATION_RESULTS.md](file:///d:/OmnixEngine/v0.3_VALIDATION_RESULTS.md#L72-L77)
* **Usable for Making Levels**: Yes. Limits are documented.
* **Details**:
  * Standalone prefab loading from disk in `PrefabRegistry::LoadPrefabFromFile` is not fully integrated.
  * Standalone prefab saving to `.omnixprefab` via `SceneSerializer::SavePrefab` is stubbed out.
  * ImGui multi-viewports dragging is disabled to prevent Vulkan surface crashes on multi-gpu systems.

### 10.5 Executable Output Path
* **Status**: `IMPLEMENTED`
* **Evidence**:
  * `CMakeLists.txt`
  * `v0.3_VALIDATION_RESULTS.md`
* **Usable for Making Levels**: Yes. Binaries are generated.
* **Details**: Built under `./build_ninja/Application.exe` or `./build/bin/Release/Application.exe` (depending on build system parameters).
