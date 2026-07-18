# OmnixEngine — Comprehensive Architecture Audit Report
**Classification:** Internal Engineering Review  
**Engine Version:** v0.4 (Inferred from build artifacts and existing audit files)  
**Audit Date:** 2026-07-16  
**Auditor:** Antigravity (Architecture Review Agent)  
**Scope:** Complete codebase under `d:\OmnixEngine`

---

## Table of Contents
1. [Executive Summary](#1-executive-summary)
2. [Overall Engine Health](#2-overall-engine-health)
3. [Implemented Features](#3-implemented-features)
4. [Missing Features](#4-missing-features)
5. [Kernel Layer Inventory](#5-kernel-layer-inventory)
6. [Editor Layer Inventory](#6-editor-layer-inventory)
7. [Potential Game Backend Systems](#7-potential-game-backend-systems)
8. [Shipment Pipeline Status](#8-shipment-pipeline-status)
9. [Dependency Analysis](#9-dependency-analysis)
10. [Architectural Risks](#10-architectural-risks)
11. [Technical Debt](#11-technical-debt)
12. [Subsystem Readiness Matrix](#12-subsystem-readiness-matrix)
13. [Feature Completeness Matrix](#13-feature-completeness-matrix)
14. [Recommended Layer Migration Plan](#14-recommended-layer-migration-plan)
15. [Critical Issues](#15-critical-issues)
16. [Suggested Immediate Priorities](#16-suggested-immediate-priorities)
17. [Suggested Medium-Term Priorities](#17-suggested-medium-term-priorities)
18. [Suggested Long-Term Priorities](#18-suggested-long-term-priorities)
19. [Overall Production Readiness Score](#19-overall-production-readiness-score)
20. [Overall Engine Maturity Assessment](#20-overall-engine-maturity-assessment)

---

## 1. Executive Summary

OmnixEngine is an ambitious, actively-developed custom game engine targeting a Vulkan-first, deferred-rendering pipeline on Windows. The engine is architecturally split across multiple high-level domains: a low-level Vulkan abstraction layer (`RenderingEngine/`), a high-fidelity rendering pipeline (`Rendering/`), a scene/ECS layer (`Scene/`, `ECS/`, `Components/`, `Systems/`), a unified runtime (`Runtime/`), and game-level systems (`Runtime/Gameplay/`). A significant portion of the rendering subsystem is **production-quality** for a v0.4 milestone, including GPU-driven culling, visibility buffers, geometry streaming, and a functioning deferred lighting chain.

**Critical observation:** The engine currently contains **two distinct rendering backends** (`RenderingEngine/Renderer/` legacy + `Rendering/Core/Renderer.cpp` active), **two ECS implementations** (the `ECS/` folder classic-style ECS with `Coordinator.h`, and the `Serializer/ECS/` serialization-aware schema-driven ECS), and numerous **stub classes** in the new `Rendering/` subsystems (e.g., `DeferredLightingPass`, `ShadowRenderer`, `TonemapPass`, `ExposurePass` — all single-method stubs of `static void Record(VkCommandBuffer cmd)`). The main rendering work lives monolithically in `Rendering/Core/Renderer.cpp` (395KB, ~10,000 lines — a God Object).

The engine is not yet a shipping product but is well above a prototype stage in its rendering and asset pipeline capabilities. It would benefit from an urgent architectural reorganization to separate its layering concerns before the codebase becomes unmaintainable.

**Overall Production Readiness Score: 2.7 / 5** (Functional–Stable boundary)

---

## 2. Overall Engine Health

| Dimension | Score (1–5) | Notes |
|---|---|---|
| Rendering Pipeline | 4 | GPU-driven deferred pipeline is functionally running |
| ECS / Scene | 3 | Two parallel ECS implementations; functional but redundant |
| Physics | 3 | PhysX integration working, debug draw present |
| Audio | 2 | Miniaudio backend exists; spatial audio absent |
| Asset System | 3 | Importers, registry, hot-reload all exist |
| Serialization | 3 | Binary + text + delta serializers; schema registry |
| Editor | 3 | Full ImGui-based editor with viewport, inspector, hierarchy |
| Gameplay Systems | 2 | Vertical-slice quality; not general purpose |
| Memory | 3 | Multiple custom allocators; no unified arena strategy |
| Threading / Job System | 2 | Declarations exist; implementations are stubs |
| Packaging | 2 | PackageBuilder/Manager exist; not integrated into pipeline |
| CI / Build | 2 | One GitHub Actions workflow; no test automation |
| Documentation | 2 | Numerous planning .md files; no API documentation |
| **Overall** | **2.7** | Functional but architecturally at risk |

---

## 3. Implemented Features

### 3.1 Rendering Pipeline

**Evidence:** `Rendering/Core/Renderer.cpp` (395 KB), `shaders/` (69 files including compiled `.spv`)

- **Deferred G-Buffer Pass** — `gbuffer_vert.glsl / gbuffer_frag.glsl` compiled; GBuffer pipeline in `Renderer.cpp`
- **Depth Pre-Pass** — `depth_vert.glsl / depth_indirect_vert.glsl` compiled
- **PBR Shading** — `pbr_vert.glsl / pbr_frag.glsl`; BRDF LUT in `brdf.glsl`
- **Deferred Lighting Pass** — `deferred_lighting.glsl` (24 KB shader); computed in `Renderer.cpp`
- **Cascaded Shadow Maps** — `shadow_vert.glsl`; `CascadedShadowData` struct in `Renderer.h`; 4-cascade system
- **SSAO + Blur** — `ssao_frag.glsl / ssao_blur_frag.glsl`; `SSAOSettings` struct; disabled by default per `RenderDebugConfig`
- **Screen Space Reflections (SSR)** — `ssr.comp` (12 KB); compiled to `ssr.spv`
- **Temporal Anti-Aliasing (TAA)** — `taa.comp` compiled to `taa.spv`
- **Depth of Field** — `dof.frag` compiled to `dof.spv`; `PostProcessSettings::enableDoF`
- **Post-Processing** — `postprocess_frag.glsl`: ACES tonemapping, gamma correction, color grading (lift/gamma/gain), fog, vignette, exposure
- **Transparent Pass** — `transparent_frag.glsl` (12 KB); forward-rendered transparency
- **Exposure / Auto-Exposure** — `exposure.comp`; `ExposureSettings` with auto-exposure range and adaptation speed
- **GPU-Driven Frustum Culling** — `FrustumCullPass.cpp/.h` (19 KB .cpp); `frustum_cull.comp` compiled; readback API
- **Hierarchical Z-Buffer (HZB)** — `HZBPass.cpp/.h` (22 KB .cpp); `hzb_downsample.comp / depth_to_hzb.comp`
- **GPU Occlusion Culling** — `OcclusionCullPass.cpp/.h` (21 KB .cpp); `occlusion_cull.comp` (5 KB)
- **Indirect Draw Command Build** — `IndirectCommandBuildPass.cpp/.h` (22 KB .cpp); `build_indirect_commands.comp`
- **Visibility Buffer (Nanite-style)** — `visibility_vert.glsl + visibility_frag.glsl + visibility_resolve.frag + visibility.mesh + visibility.task` — **Mesh/Task shader pipeline targeting Vulkan 1.3**; `rvg_cull.comp` (12 KB)
- **Software Rasterizer (GPU)** — `software_rasterizer.comp` (5 KB, 15 KB compiled SPIR-V)
- **GPU Scene** — `GPUScene.cpp` (93 KB); stable handle allocation, entity-to-instance mapping, material overrides, clustered lighting
- **Clustered Lighting** — `light_culling.comp`; cluster bounds/range/index buffers in `GPUSceneFrameResources`
- **Environment System** — `EnvironmentSystem.cpp` (19 KB); HDR cubemap loading, irradiance/prefilter cube, BRDF LUT
- **Sky / Atmosphere** — `RadianceSettings` with procedural sky (top/horizon/ground), sun direction/intensity; `Omnix::Radiance` namespace
- **Grid Editor Overlay** — `grid_vert.glsl / grid_frag.glsl`; `GridRenderer`
- **Selection Outline** — `selection_outline_frag.glsl`; `SelectionOutlinePass.cpp` (8 KB)
- **Render Graph** — `Rendering/Graph/RenderGraph.cpp` (28 KB); pass/resource node system
- **Render Target Manager** — `Rendering/Core/RenderTargetManager.cpp` (16 KB)
- **Framebuffer Manager** — `Rendering/Core/FramebufferManager.cpp` (8 KB)
- **RVG Geometry System** — Renderable Virtual Geometry: clustered mesh format, LOD hierarchy, page streaming (see §3.2)
- **Geometry Arena** — `GeometryArena.cpp` (13 KB); pool-based GPU geometry allocation
- **Capability Tier System** — `CapabilityTiers.cpp` (7 KB); hardware-capability-based tier detection (0–3)

### 3.2 Geometry / Streaming (RVG System)

**Evidence:** `Rendering/Geometry/`, `Tools/RVGCooker/`, `Rendering/Geometry/Streaming/`

- **RVGCooker** — Offline mesh preprocessing tool: `MeshCanonicalizer`, `MeshAdjacency`, `ClusterBuilder`, `ClusterGroupBuilder`, `ClusterSimplifier`, `HierarchyBuilder`, `RVGPagePacker`, `RVGWriter` — a full Nanite-style geometry pipeline preprocessing tool
- **RVGAsset / RVGRegistry** — Runtime GPU cluster asset management
- **RVGPageStreamingManager** — Full background-threaded streaming manager with priority queue, LRU physical page eviction, GPU page table, staging buffer uploads
- **RVGClusterCullPass** — `rvg_cull.comp` (28 KB SPIR-V); cluster-level visibility culling

### 3.3 Scene & ECS

**Evidence:** `Scene/`, `ECS/`, `Components/`, `Systems/`

- **Scene** — `Scene.cpp` (31 KB), `SceneManager.cpp` (31 KB); scene CRUD, entity hierarchy
- **SceneObject** — `SceneObject.cpp` (12 KB); entity-wrapping scene node
- **SceneLoader** — `SceneLoader.cpp` (39 KB); JSON-based scene loading
- **SceneSerializer** — `SceneSerializer.cpp` (27 KB); round-trip scene save/load
- **SceneValidator** — `SceneValidator.cpp` (27 KB)
- **Prefab System** — `Prefab.cpp` (7 KB), `PrefabRegistry.cpp` (6 KB)
- **ComponentFactory** — `ComponentFactory.cpp` (12 KB)
- **ECS** — `Coordinator.h` (7 KB); `ComponentManager`, `SystemManager`, `EntityManager`; archetype-style with signature-based queries
- **ECS Systems** — `BoundsUpdateSystem.h` (7 KB), `CameraSystem.h`, `PhysicsSystem.h`, `PlayerControllerSystem.h` (8 KB), `TriggerSystem.h` (18 KB), `LightCollectionSystem.h` (9 KB), `RenderSystem.h`, `PlayerSystem.h`
- **Transform System** — `Transform.cpp` (5 KB), local/world decomposition
- **Camera** — `Camera.cpp` (8 KB), perspective/orthographic
- **World Manager** — `WorldManager.cpp` (15 KB); multi-zone world file system
- **World Zones** — `WorldZone.cpp`, `WorldZoneReader/Writer`
- **ECS Components** — Comprehensive set in `ECSComponents.h` (538 lines): Transform, Mesh, Light (Directional/Point/Spot/Sky/Reflection), Physics (RigidBody, Box/Sphere/Capsule Collider), Audio, Animation, Script, Player, Bounds, Trigger, Gameplay components

### 3.4 Physics

**Evidence:** `Physics/Private/PhysicsWorld.cpp` (17 KB), `Physics/Public/`

- **NVIDIA PhysX Integration** — Conditional via `OMNIX_WITH_PHYSX` CMake flag; PxFoundation, PxPhysics, PxScene initialized with gravity
- **PhysicsWorld** — `Initialize/Shutdown/FixedUpdate`; `PxScene` scene management
- **PhysicsDebugDraw** — `PhysicsDebugDraw.cpp` (37 KB); full Vulkan-based physics wireframe debug rendering
- **Collision Primitives** — Box, Sphere, Capsule colliders via ECS components
- **Triggers** — `TriggerSystem.h` (18 KB); full enter/stay/exit logic
- **PhysicsValidation** — `PhysicsValidation.cpp` (2 KB)

### 3.5 Asset System

**Evidence:** `Runtime/Private/`, `Runtime/Public/`

- **AssetRegistry** — `AssetRegistry.cpp` (7 KB); UUID-keyed asset metadata store; `AssetRegistry.json` at root
- **AssetManager** — `AssetManager.cpp` (10 KB); load/unload/cache lifecycle
- **AssetHandle** — Type-safe `uint64_t` handle with generation
- **GLTF Importer** — `GLTFImporter.cpp` (9 KB); via `tinygltf`
- **OBJ Importer** — `OBJImporter.cpp` (7 KB)
- **Mesh Importer** — `MeshImporter.cpp` (15 KB); produces `.omxmesh` binary
- **Texture Importer** — `TextureImporter.cpp` (11 KB); produces `.omxtex`
- **Hot Reload** — `HotReloadSystem.cpp` (9 KB) + `FileWatcher.cpp`; topological dependency-ordered reload
- **Package System** — `PackageBuilder.cpp`, `Package.cpp`, `PackageManager.cpp`; mounts/unmounts binary asset packages
- **Binary Format** — `OmnixMeshFormat.h`, `OmnixMaterialFormat.h`, `OmnixTextureFormat.h`, `OmnixSceneFormat.h`, `OmnixAnimFormat.h`, `OmnixPackageFormat.h`; checksum-validated binary formats
- **TextureCache** — `TextureCache.h`; GPU texture cache system

### 3.6 Serialization

**Evidence:** `Serializer/`, `Serializer/Serialization/`

- **Normal Serializer/Deserializer** — Text (JSON) and Binary backends
- **Delta Serializer** — `DeltaSerializer/DeltaDeserializer/DeltaTracker/DeltaSnapshot`; incremental field-level delta tracking
- **ECS Snapshot System** — `ComponentSnapshot`, `FieldSnapshot`, `ECSSnapshot`; full ECS state snapshots
- **Schema Registry** — `SchemaRegistry.cpp` (29 bytes — stub) / `SchemaRegistry.h` (2 KB)
- **SerializationBridge** — Bridges ECS components to the serialization system
- **Binary Reader/Writer** — `BinaryWriter.cpp / BinaryReader.cpp` with checksum validation

### 3.7 Reflection

**Evidence:** `Runtime/Public/Reflection.h`

- **ReflectionRegistry** — Singleton; `RegisterType<T>`, `RegisterField<T,F>` by offset
- **Macros** — `REFLECT_STRUCT_BEGIN / REFLECT_FIELD / REFLECT_STRUCT_END`
- **Property Flags** — `Edit | Save | Network`
- **Get/Set** — `SetProperty / GetProperty` template functions

### 3.8 Memory

**Evidence:** `Core/Memory/`, `RenderingEngine/Core/memory/`

**Core Layer:**
- `LinearAllocator` — `LinearAllocator.cpp` (2 KB); simple bump allocator
- `PoolAllocator` — `PoolAllocator.cpp` (3 KB); fixed-size pool
- `StackAllocator` — `StackAllocator.cpp` (2 KB)
- `FreeListAllocator` — `FreeListAllocator.cpp` (6 KB); general-purpose free list
- `AllocationTracker` — `AllocationTracker.cpp` (7 KB); allocation tracking with size statistics
- `AllocatorValidation` — `AllocatorValidation.cpp` (15 KB); correctness validation suite
- `GenerationTaggedHandle` — Type-safe generational handles

**RenderingEngine Layer:**
- `LinearAllocator.h` (10 KB); frame-scoped linear allocator
- `PoolAllocator.h`, `RingAllocator.h` (4 KB), `MemoryTracker.h`

### 3.9 Logging

**Evidence:** `Core/Logging/Logger.cpp` (4 KB), `Core/Logging/Logger.h` (5 KB)

- Multi-category logging (`LogCategory.h`)
- Severity levels (`LogSeverity.h`): Trace, Debug, Info, Warning, Error, Fatal
- File output to `Omnix.log` (currently 492 MB — indicates heavy development logging)
- `Logger::Init / Shutdown` lifecycle

### 3.10 Input System

**Evidence:** `Input/InputManager.cpp` (5 KB), `RenderingEngine/Platform/Input/`

- **InputManager** — Keyboard, mouse, gamepad abstraction
- **Win32 Backend** — `InputWin32.cpp` (12 KB); raw Win32 WM_INPUT processing
- **Input Events** — `InputEvent.h`, `InputEventTypes.h`
- **Input Binding** — `InputBinding.h`; action-to-key mapping (declared, partial)

### 3.11 Window System

**Evidence:** `RenderingEngine/Platform/window/Window.cpp`, `WindowWin32.h` (19 KB)

- Win32 window creation and management
- GLFW abstraction layer alongside Win32

### 3.12 Editor

**Evidence:** `Runtime/Private/Editor/` (~200 KB total)

- **EditorLayer** — `EditorLayer.cpp` (167 KB); main editor orchestration — largest file in the codebase
- **SceneHierarchyPanel** — `SceneHierarchyPanel.cpp` (16 KB); entity tree, drag-drop, selection
- **InspectorPanel** — `InspectorPanel.cpp` (51 KB); full component inspector with property editing
- **ViewportPanel** — `ViewportPanel.cpp` (77 KB); offscreen viewport rendering, gizmos, picking
- **AssetBrowserPanel** — `AssetBrowserPanel.cpp` (7 KB); asset filesystem navigation
- **ConsolePanel** — `ConsolePanel.cpp` (395 bytes — minimal stub)
- **ImportLogPanel** — `ImportLogPanel.cpp` (1 KB)
- **ComponentWidgets** — `ComponentWidgets.cpp` (63 KB); per-component ImGui property widgets
- **TransformWidget** — `TransformWidget.cpp` (3 KB)
- **EditorCamera** — `EditorCamera.h` (8 KB); fly camera with orbit, pan, zoom
- **EditorTheme** — `EditorTheme.cpp` (6 KB); ImGui styling
- **EditorSelection** — `EditorSelection.cpp`; multi-select entity management
- **EditorEntityCommands** — `EditorEntityCommands.cpp` (12 KB); command pattern for undo/redo
- **EditorSceneService** — `EditorSceneService.cpp` (8 KB); scene CRUD integration
- **EditorFileService** — `EditorFileService.cpp` (9 KB); file system integration
- **AssetImportService** — `AssetImportService.cpp` (16 KB); drag-drop asset import pipeline
- **EditorNotificationService** — `EditorNotificationService.cpp` (3 KB)
- **PlatformFileDialog** — Win32 open/save file dialog
- **ImGuizmo Integration** — Gizmo manipulation (translate, rotate, scale)
- **EditorOverlayPass** — `EditorOverlayPass.cpp` (2 KB)
- **EditorViewportRenderer** — `EditorViewportRenderer.cpp` (15 KB)
- **DebugViewRenderer** — stub (205 bytes .cpp)
- **SelectionOutlinePass** — `SelectionOutlinePass.cpp` (8 KB)

### 3.13 Audio

**Evidence:** `Runtime/Private/Audio/AudioSystem.cpp` (20 KB), `Runtime/Public/Audio/`

- **miniaudio backend** — `ma_engine`, `ma_sound` structs; loaded from `miniaudio/miniaudio.h`
- **AudioSystem** — Initialization, shutdown, `PlayOneShot`, `StopSound`, `StopAllSounds`
- **Entity Sound Tracking** — Entity-ID-to-sound mapping for per-entity playback
- **Volume Control** — Master volume API
- **AudioSourceComponent** — Integrated with ECS
- `MiniaudioBackend.cpp` — 66 bytes (near-stub)

### 3.14 Event System

**Evidence:** `EventManagement/`, `Runtime/Public/EventBus.h`

- **EventManager** — `EventManager.h` (7 KB); typed event subscription and dispatch
- **GameEvent** — `GameEvent.h` (7 KB); polymorphic event base
- **EventQueue** — `EventQueue.h` (2 KB)
- **EventBus** — `EventBus.h` (3 KB); async event bus
- **GameplayEventBus** — `GameplayEventBus.cpp` (2 KB)
- **Event Types** — `EntityEventTypes.h`, `InputEventTypes.h`, `PhysicsEventTypes.h`, `SceneEventTypes.h`, `GameplayEvent.h`

### 3.15 Configuration / CVar / Console

**Evidence:** `Runtime/Public/CVarSystem.h`, `Runtime/Public/ConfigSystem.h`, `Runtime/Public/RuntimeConsole.h`

- **CVarSystem** — `CVarSystem.cpp` (7 KB); runtime console variables (get/set/list)
- **ConfigSystem** — `ConfigSystem.cpp` (9 KB); JSON-based project configuration
- **RuntimeConsole** — `RuntimeConsole.cpp` (9 KB); console command registration and dispatch

### 3.16 Module / Plugin System

**Evidence:** `Runtime/Public/PluginManager.h`, `Runtime/Public/ModuleManager.h`

- **PluginManager** — DLL loading, ABI version checking (`ENGINE_ABI_VERSION = 0x00010000`), module instantiation/unloading
- **ModuleManager** — `ModuleManager.cpp` (4 KB); manages registered IModule instances
- **ServiceRegistry** — `ServiceRegistry.h` (2 KB); IoC service locator

### 3.17 Gameplay Systems

**Evidence:** `Runtime/Private/Gameplay/`, `Runtime/Public/Gameplay/`

- **GameMode** — `GameMode.cpp` (12 KB); abstract game mode lifecycle
- **VerticalSliceGameMode** — `VerticalSliceGameMode.cpp` (564 bytes); project-specific game mode
- **CheckpointSystem** — `CheckpointSystem.cpp` (13 KB)
- **InteractionSystem** — `InteractionSystem.cpp` (9 KB)
- **ObjectiveSystem** — `ObjectiveSystem.cpp` (under `Objectives/`)
- **ObjectActivationSystem** — `ObjectActivationSystem.cpp`
- **GameplaySaveSystem** — `GameplaySaveSystem.cpp` (25 KB); full save/load with JSON persistence
- **GameplayHUD** — `GameplayHUD.cpp` (under `UI/`)
- **GameplayValidator** — `GameplayValidator.cpp`
- **PlayerStateComponent** — Player stats
- **GameState / GameSessionState** — Runtime game state

### 3.18 World / Zone System

**Evidence:** `Runtime/Private/World/`, `Runtime/Public/World/`

- **WorldManager** — `WorldManager.cpp` (15 KB); multi-zone world management
- **WorldFileReader/Writer** — Binary world file serialization
- **WorldZone / WorldZoneReader/Writer** — Zone subdivision of the world
- **OmnixWorldHeader / OmnixZoneHeader** — Binary format headers

### 3.19 Diagnostics / Testing

**Evidence:** `Core/Diagnostics/StressTest.cpp` (125 KB!), `Runtime/Private/*Tests.cpp`

- **StressTest** — `StressTest.cpp` (125 KB); comprehensive stress test suite
- **AllocatorValidation** — `AllocatorValidation.cpp` (15 KB)
- **Asset Tests** — `AssetCacheTests.cpp` (16 KB), `AssetRegistryTests.cpp` (9 KB), `AssetLoadingStressTests.cpp` (9 KB)
- **Format Tests** — `FormatTests.cpp` (112 KB!); comprehensive binary format round-trip tests
- **GPU Scene Tests** — `GPUSceneTests.cpp` (15 KB)
- **Golden Image Tests** — `GoldenImageTests.cpp` (6 KB)
- **Geometry Tests** — `GeometryArenaTests.cpp`, `GeometryHandleTests.cpp`
- **Hot Reload Tests** — `HotReloadStressTests.cpp` (8 KB), `TextureReloadTests`, `ShaderReloadTests`, `MeshReloadTests`
- **Package Tests** — `PackageTests.cpp` (22 KB)
- **Transform Tests** — `transform_tests` executable from `Scene/tests/TransformTests.cpp`

---

## 4. Missing Features

### Confirmed Absent (No Files Found)

| Feature | Status | Evidence of Absence |
|---|---|---|
| Animation Runtime | Not Started | `AnimatorComponent` defined but no animation system exists; `OmnixAnimFormat.h` exists as a format spec only |
| Skeletal Mesh Rendering | Not Started | `OmnixSkinnedVertex` struct defined but no skinning shader found |
| Navigation / NavMesh | Not Started | `NavigationAgent.h` is a zero-byte stub in `Components/Behavior/` |
| AI Controller | Not Started | `AIController.h` is a zero-byte stub in `Components/Behavior/` |
| Networking | Not Started | No networking directory, no network headers found |
| Scripting Runtime | Not Started | `ScriptComponent` in ECS but no script VM or Lua/Python binding found |
| Terrain System | Not Started | No terrain files found |
| Particle System | Not Started | No particle emitter or GPU particle system found |
| UI Runtime (game UI) | Partial | `GameplayHUD.cpp` exists; no general UI widget framework |
| Material Graph Editor | Not Started | No visual material editor |
| Animation Editor | Not Started | No animation editor panel |
| Prefab Editor | Not Started | Prefab system exists; no dedicated editor panel |
| Shader Hot Reload | Partial | `ShaderReloadTests.cpp` exists; full implementation unclear |
| Audio Spatialization | Not Started | Miniaudio used but no 3D spatial audio API |
| Cloth Simulation | Not Started | `SoftBody.h` is a zero-byte stub |
| Destruction System | Not Started | No destructible geometry system |
| Streaming Level Loading | Partial | WorldZone system exists; async zone loading incomplete |
| GPU Profiler | Stub | `GPUProfiler.h/.cpp` in `Rendering/Debug/` — 235-byte header, 238-byte .cpp (stub only) |
| RenderDoc Integration | Partial | `RequestRenderDocCapture()` method declared in `Renderer.h`; integration depth unknown |
| Cross-Platform Support | Not Started | Win32-only; GLFW present but all backends are Win32 |
| Linux/Mac Window Backend | Not Started | `WindowWin32.h` only; no POSIX layer |
| Console Platform Support | Not Started | No platform abstraction beyond Win32 |
| Automated CI Testing | Not Started | CI workflow only builds; no test execution in CI |

### Stub Classes (Headers/Empty .cpp Only)

| Class | Location | Status |
|---|---|---|
| `DeferredLightingPass` | `Rendering/Lighting/` | Stub — one `static void Record(VkCommandBuffer)` |
| `ShadowRenderer` | `Rendering/Lighting/` | Stub — one `static void Record(VkCommandBuffer)` |
| `SkyLightRenderer` | `Rendering/Lighting/` | Stub — one `static void Record(VkCommandBuffer)` |
| `TonemapPass` | `Rendering/PostProcess/` | Stub — one `static void Record(VkCommandBuffer)` |
| `ExposurePass` | `Rendering/PostProcess/` | Stub — one `static void Record(VkCommandBuffer)` |
| `DebugViewRenderer` | `Rendering/Editor/` | Stub — 205 bytes .cpp |
| `GridRenderer` | `Rendering/Editor/` | Stub — 233 bytes .cpp |
| `MaterialSystem` | `Rendering/Materials/` | Stub — `MaterialSystem.cpp` is 526 bytes |
| `MeshRenderer` | `Rendering/Geometry/` | Stub — `MeshRenderer.cpp` is 5 KB |
| `VisibilitySystem` (Geometry) | `Rendering/Geometry/` | Stub — 264 bytes .cpp |
| `GPUProfiler` | `Rendering/Debug/` | Stub — 238 bytes .cpp |
| `SchemaRegistry` | `Serializer/ECS/` | Near-stub — 29 bytes .cpp |
| `MiniaudioBackend` | `Runtime/Private/Audio/` | Near-stub — 66 bytes .cpp |
| `ConsolePanel` | `Runtime/Private/Editor/Panels/` | Near-stub — 395 bytes .cpp |
| `VisibilitySystem` (Geometry) | `Rendering/Geometry/` | Stub |

### Legacy Rendering Engine (Mostly Dead Code)

The `RenderingEngine/Renderer/Techniques/` directory contains:
- `ClusteredRender.cpp` — zero bytes
- `DefferedRenderer.cpp` — zero bytes
- `ForwardRenderer.cpp` — zero bytes

The `RenderingEngine/Renderer/Passes/` directory:
- `DebugPass.cpp`, `DepthPrePass.cpp`, `GBufferPass.cpp`, `LightingPass.cpp`, `PostProcessingPass.cpp`, `ShadowPass.cpp`, `SkyPass.cpp`, `UIPass.cpp` — all zero bytes

These represent the original rendering architecture that has been superseded by the `Rendering/Core/Renderer.cpp` monolith.

---

## 5. Kernel Layer Inventory

The kernel layer encompasses all engine systems that must function independently of any game or editor logic.

### 5.1 Core

| Subsystem | Location | Status | Production Readiness | Independence |
|---|---|---|---|---|
| Logger | `Core/Logging/` | Mostly Complete | Level 3 | ✅ Fully independent |
| Memory Allocators | `Core/Memory/` | Mostly Complete | Level 3 | ✅ Fully independent |
| Timer | `Core/Timer.h/.cpp` | Mostly Complete | Level 3 | ✅ |
| Diagnostics | `Core/Diagnostics/` | Partial | Level 2 | ✅ |
| Application | `Core/Application.cpp` | Partial | Level 2 | ✅ |

### 5.2 Vulkan / RHI

| Subsystem | Location | Status | Production Readiness | Notes |
|---|---|---|---|---|
| VulkanInstance | `RenderingEngine/Vulkan/VulkanInstance.cpp` | Mostly Complete | Level 3 | Extension/validation layer setup |
| VulkanDevice | `RenderingEngine/Vulkan/VulkanDevice.cpp` | Mostly Complete | Level 3 | Physical/logical device selection |
| VulkanSwapChain | `RenderingEngine/Vulkan/VulkanSwapChain.cpp` | Mostly Complete | Level 3 | Swapchain/recreation |
| VulkanMemory (VMA) | `RenderingEngine/Vulkan/VulkanMemory.cpp` | Partial | Level 2 | 79-byte .cpp — primarily header-based |
| RHI Abstraction (New) | `Rendering/RHI/` | Prototype | Level 1 | Declared but no implementations; 6 tiny headers |
| RHI Abstraction (Legacy) | `RenderingEngine/rhi/` | Prototype | Level 1 | 13 header stubs; no .cpp files |
| EngineResources | `RenderingEngine/Core/Engine/EngineResources.cpp` | Mostly Complete | Level 3 | Central Vulkan resource aggregator |
| FrameGraph (New) | `RenderingEngine/Runtime/frameGraph/` | Prototype | Level 1 | Headers-only; `GPUCompiler.h / GPUExecutor.h` — zero bytes |
| RenderGraph (Active) | `Rendering/Graph/RenderGraph.cpp` | Partial | Level 2 | 28 KB implementation in active use |

**Critical Note:** There are three layers of RHI/abstraction:
1. `RenderingEngine/rhi/` — legacy stub headers
2. `Rendering/RHI/` — new stub headers (e.g., `RHIBuffer.h`, `RHIDevice.h` — all tiny)
3. `EngineResources` — the actual working Vulkan abstraction layer

None of the formal RHI abstractions have implementations. All real Vulkan calls go directly through `EngineResources` and raw Vulkan handles scattered throughout `Renderer.cpp`.

### 5.3 Rendering Core

| Subsystem | Location | Status | Readiness | Notes |
|---|---|---|---|---|
| Renderer (God Object) | `Rendering/Core/Renderer.cpp` | Mostly Complete | Level 3 | 395 KB monolith — critical architectural risk |
| GPUScene | `Rendering/GPUScene/GPUScene.cpp` | Mostly Complete | Level 3 | Well-designed; stable handles |
| RenderTargetManager | `Rendering/Core/RenderTargetManager.cpp` | Mostly Complete | Level 3 | |
| FramebufferManager | `Rendering/Core/FramebufferManager.cpp` | Mostly Complete | Level 2 | |
| FrustumCullPass | `Rendering/Visibility/FrustumCullPass.cpp` | Mostly Complete | Level 3 | GPU compute |
| HZBPass | `Rendering/Visibility/HZBPass.cpp` | Mostly Complete | Level 3 | |
| OcclusionCullPass | `Rendering/Visibility/OcclusionCullPass.cpp` | Mostly Complete | Level 3 | |
| IndirectCommandBuildPass | `Rendering/Visibility/IndirectCommandBuildPass.cpp` | Mostly Complete | Level 3 | |
| RVGClusterCullPass | `Rendering/Visibility/RVGClusterCullPass.cpp` | Mostly Complete | Level 2 | |
| GeometryArena | `Rendering/Geometry/Arena/` | Mostly Complete | Level 3 | |
| RVGPageStreamingManager | `Rendering/Geometry/Streaming/` | Partial | Level 2 | Background thread; complex interaction |
| CapabilityTiers | `Rendering/Geometry/CapabilityTiers.cpp` | Mostly Complete | Level 3 | |
| EnvironmentSystem | `Rendering/Lighting/EnvironmentSystem.cpp` | Mostly Complete | Level 2 | HDR loading working |
| SelectionOutlinePass | `Rendering/Editor/SelectionOutlinePass.cpp` | Mostly Complete | Level 2 | Editor-coupled |
| EditorViewportRenderer | `Rendering/Editor/EditorViewportRenderer.cpp` | Mostly Complete | Level 2 | Editor-coupled |
| DebugDraw | `Rendering/Debug/DebugDraw.cpp` | Partial | Level 2 | 3 KB impl |
| MaterialSystem | `Rendering/Materials/MaterialSystem.cpp` | Stub | Level 1 | 526-byte stub |
| DeferredLightingPass | `Rendering/Lighting/DeferredLightingPass.cpp` | Stub | Level 1 | Logic in Renderer.cpp |
| ShadowRenderer | `Rendering/Lighting/ShadowRenderer.cpp` | Stub | Level 1 | Logic in Renderer.cpp |
| TonemapPass | `Rendering/PostProcess/TonemapPass.cpp` | Stub | Level 1 | Logic in Renderer.cpp |

### 5.4 Geometry Preprocessing (Offline Tool)

| Tool | Location | Status | Readiness |
|---|---|---|---|
| RVGCooker | `Tools/RVGCooker/` | Mostly Complete | Level 3 |
| ClusterBuilder | `Tools/RVGCooker/ClusterBuilder.cpp` | Mostly Complete | Level 3 |
| ClusterSimplifier | `Tools/RVGCooker/ClusterSimplifier.cpp` | Mostly Complete | Level 3 |
| HierarchyBuilder | `Tools/RVGCooker/HierarchyBuilder.cpp` | Mostly Complete | Level 3 |

---

## 6. Editor Layer Inventory

All editor systems live under `Runtime/Private/Editor/` and `Runtime/Public/Editor/`.

| Panel / System | Location | Status | Readiness | Notes |
|---|---|---|---|---|
| EditorLayer (Orchestrator) | `EditorLayer.cpp` (167 KB) | Mostly Complete | Level 3 | Very large; God Object risk |
| SceneHierarchyPanel | `Panels/SceneHierarchyPanel.cpp` | Mostly Complete | Level 3 | Drag-drop, multi-select |
| InspectorPanel | `Panels/InspectorPanel.cpp` (51 KB) | Mostly Complete | Level 3 | All core components exposed |
| ViewportPanel | `Panels/ViewportPanel.cpp` (77 KB) | Mostly Complete | Level 3 | Offscreen rendering, picking, gizmos |
| AssetBrowserPanel | `Panels/AssetBrowserPanel.cpp` | Partial | Level 2 | Basic navigation |
| ConsolePanel | `Panels/ConsolePanel.cpp` | Stub | Level 1 | 395 bytes |
| ImportLogPanel | `Panels/ImportLogPanel.cpp` | Partial | Level 2 | Shows import events |
| ComponentWidgets | `Widgets/ComponentWidgets.cpp` (63 KB) | Mostly Complete | Level 3 | Per-type UI widgets |
| TransformWidget | `Widgets/TransformWidget.cpp` | Mostly Complete | Level 3 | |
| EditorCamera | `Runtime/Public/Editor/EditorCamera.h` | Mostly Complete | Level 3 | Fly/orbit/pan |
| EditorTheme | `EditorTheme.cpp` (6 KB) | Partial | Level 2 | ImGui dark theme |
| EditorEntityCommands | `Commands/EditorEntityCommands.cpp` (12 KB) | Mostly Complete | Level 2 | Create/delete/duplicate |
| Gizmos | ImGuizmo integration | Mostly Complete | Level 3 | Transform manipulation |
| EditorSelection | `EditorSelection.cpp` | Partial | Level 2 | Entity selection tracking |
| AssetImportService | `AssetImportService.cpp` (16 KB) | Mostly Complete | Level 2 | Drag-drop import pipeline |
| EditorSceneService | `EditorSceneService.cpp` (8 KB) | Mostly Complete | Level 2 | New/open/save scene |
| EditorFileService | `EditorFileService.cpp` (9 KB) | Mostly Complete | Level 2 | File dialogs |
| EditorDirtyState | `EditorDirtyState.cpp` (359 bytes) | Partial | Level 2 | Unsaved-changes tracking |
| EditorNotificationService | `EditorNotificationService.cpp` (3 KB) | Partial | Level 2 | Toast notifications |
| PlatformFileDialog | `PlatformFileDialog.cpp` (1 KB) | Partial | Level 2 | Win32 dialogs |
| EditorLayout | `EditorLayout.cpp` (1 KB) | Partial | Level 2 | Docking persistence |
| Grid Overlay | `Rendering/Editor/GridRenderer.cpp` | Stub | Level 1 | Logic in Renderer.cpp |
| Debug View | `Rendering/Editor/DebugViewRenderer.cpp` | Stub | Level 1 | |
| EditorOverlayPass | `Rendering/Editor/EditorOverlayPass.cpp` (2 KB) | Partial | Level 2 | |

**Missing Editor Features:**
- Material Editor (no panel)
- Animation Editor (no panel)  
- Prefab Editor (no dedicated panel)
- Profiler Panel (GPUProfiler is a stub)
- Terrain Editor
- Statistics overlay (Partial — stats exposed via ViewportPanel)
- Undo/Redo system (Commands exist; full undo stack not confirmed)
- Content Browser (AssetBrowserPanel is basic)
- Particle Editor

---

## 7. Potential Game Backend Systems

These are runtime systems that belong in a "game backend" or "product" layer — above kernel, below a specific product.

| System | Location | Status | Layer Recommendation |
|---|---|---|---|
| GameMode | `Runtime/Private/Gameplay/GameMode.cpp` | Partial | Game Backend |
| VerticalSliceGameMode | `Runtime/Private/Gameplay/` | Prototype | Product (project-specific) |
| CheckpointSystem | `Runtime/Private/Gameplay/CheckpointSystem.cpp` | Partial | Game Backend |
| InteractionSystem | `Runtime/Private/Gameplay/Systems/InteractionSystem.cpp` | Partial | Game Backend |
| ObjectiveSystem | `Runtime/Private/Gameplay/Objectives/` | Partial | Game Backend |
| GameplaySaveSystem | `Runtime/Private/Gameplay/Save/` | Mostly Complete | Game Backend |
| GameplayHUD | `Runtime/Private/Gameplay/UI/` | Partial | Product |
| GameplayEventBus | `Runtime/Private/Gameplay/` | Mostly Complete | Game Backend |
| PlayerStateComponent | `Runtime/Public/Gameplay/` | Partial | Game Backend |
| StateObjects (Door, Activatable, SimpleState) | `Runtime/Public/Gameplay/StateObjects/` | Partial | Product |
| WorldManager / WorldZone | `Runtime/Private/World/` | Mostly Complete | Game Backend |
| AudioSystem | `Runtime/Private/Audio/` | Partial | Game Backend |
| PhysicsWorld | `Physics/Private/` | Mostly Complete | Kernel |

---

## 8. Shipment Pipeline Status

| Capability | Status | Evidence |
|---|---|---|
| Project creation | **Missing** | No project wizard or project template system |
| Asset importing | **Implemented** | `AssetImportService.cpp`, GLTF/OBJ/Texture importers |
| Asset management | **Partial** | AssetRegistry + AssetManager exist; no asset metadata editor |
| Scene editing | **Implemented** | Full editor with hierarchy, inspector, viewport |
| Serialization | **Implemented** | Binary + JSON serializers; format headers defined |
| Packaging | **Partial** | `PackageBuilder/Manager` exist; not integrated in editor |
| Cooking assets | **Partial** | `RVGCooker` tool exists as standalone executable |
| Runtime loading | **Implemented** | RuntimeLoaders, PackageManager mount/unmount |
| Hot reload | **Implemented** | HotReloadSystem with FileWatcher and topological ordering |
| Editor launch | **Implemented** | Single `main()` entry boots editor by default |
| Standalone runtime | **Missing** | No separate runtime-only launch mode; editor always included |
| Debug runtime | **Unknown** | No explicit debug launch configuration |
| Release runtime | **Partial** | CMake Release config with `/O2 /arch:AVX2`; no strip/sign |
| Plugin loading | **Partial** | PluginManager with ABI check; DLL loading only, no discovery |
| Configuration management | **Implemented** | ConfigSystem (JSON) + CVarSystem |
| Save/Load (game) | **Implemented** | GameplaySaveSystem (25 KB) |
| Logging | **Implemented** | Logger with categories, severity, file output |
| Crash handling | **Partial** | `try/catch` in `main()`; no minidump or crash reporter |
| Profiling | **Stub** | GPUProfiler stub; no CPU profiler integration |
| Performance analysis | **Unknown** | No Pix/RenderDoc/Tracy integration confirmed |
| Automated testing | **Partial** | Test .cpp files exist; no CI test execution |
| Build automation | **Partial** | CMake build; one CI workflow that only builds |
| Continuous integration | **Partial** | GitHub Actions on push/PR to main; no test stage |
| Cross-platform abstraction | **Not Started** | Win32 only |
| Game packaging | **Not Started** | No packaging pipeline; no installer/bundle |
| Executable generation | **Implemented** | CMake produces `Application.exe` |
| Distribution readiness | **Not Started** | No asset cooking, no launcher, no installer |

---

## 9. Dependency Analysis

### 9.1 Build Dependency Graph

```
EngineCore
  └── Core/Logging, Core/Memory, Core/Timer, Core/Diagnostics

Serialization
  └── EngineCore

ECS
  └── EngineCore, Serialization

Input
  └── EngineCore

Scene
  └── EngineCore, ECS

Physics
  └── EngineCore, ECS, [NVIDIA PhysX SDK]

RenderingEngine
  └── EngineCore, Vulkan, GLFW, imgui,
      Rendering/Core/Renderer.cpp,
      Rendering/Graph/, Rendering/GPUScene/,
      Rendering/Visibility/, Rendering/Geometry/,
      Rendering/Materials/, Rendering/Lighting/,
      Rendering/PostProcess/, Rendering/Editor/,
      Rendering/Debug/,
      RenderingEngine/Vulkan/,
      RenderingEngine/Core/Engine/,
      RenderingEngine/Renderer/scene/ (legacy scene objects),
      RenderingEngine/Renderer/gltf/,
      RenderingEngine/Platform/

EngineRuntime
  └── EngineCore, ECS, Serialization, Input, Scene,
      RenderingEngine, imgui, Physics,
      Runtime/Private/Editor/ (ALL editor code),
      Runtime/Private/Gameplay/ (ALL gameplay code),
      Runtime/Private/Audio/,
      Runtime/Private/World/,
      Runtime/Private/ (asset system, hot reload, packages, tests)

Application
  └── EngineRuntime, GLFW
```

### 9.2 Identified Dependency Problems

#### Illegal Dependencies (Architectural Violations)

| Problem | Description | Severity |
|---|---|---|
| Editor code inside `EngineRuntime` | All editor panels, services, widgets compiled into the runtime library | **Critical** |
| Gameplay code inside `EngineRuntime` | VerticalSliceGameMode, CheckpointSystem, InteractionSystem, etc. compiled into runtime | **High** |
| Test code inside `EngineRuntime` | `FormatTests.cpp` (112 KB!), `StressTest.cpp` (125 KB!), GPU tests — all compiled into the production library | **High** |
| Renderer.cpp includes editor paths | `#include "Rendering/Editor/EditorViewportRenderer.h"` inside the runtime renderer | **High** |
| `EngineResources` tightly couples Vulkan | Every rendering subsystem accepts `EngineResources&` — no RHI abstraction | **Medium** |
| `Scene.h` knows about `SceneObject.h` | The scene has direct knowledge of concrete scene objects | **Medium** |
| Physics directly includes ECS | `PhysicsWorld.cpp` includes `ECS/Coordinator.h` | **Medium** |

#### Circular / Bidirectional Coupling Risks

| Pair | Type | Notes |
|---|---|---|
| `Renderer.cpp` ↔ `GPUScene` | Bidirectional | Renderer owns GPUScene but GPUScene calls back into Renderer state |
| `ECSComponents.h` includes `Runtime/Public/` | Forward coupling | ECS header pulls in runtime types |
| `Scene/` ↔ `ECS/` | Tight coupling | Scene wraps ECS coordinator; ECS systems query scene |
| `EditorLayer` ↔ `EngineRuntime` | Bidirectional | Editor calls runtime; runtime holds editor reference |

#### High Coupling Areas

| Subsystem | Coupling Cause |
|---|---|
| `Renderer.cpp` (395 KB) | Owns pipelines, passes, visibility, GPU scene, post-process, editor overlay, shadow atlas — everything |
| `EngineRuntime.cpp` (36 KB) | Initializes and updates all subsystems in a single class |
| `EditorLayer.cpp` (167 KB) | Touches every other system; no clean boundary |
| `ECSComponents.h` | Aggregate header pulling in gameplay, physics, audio, rendering components |

---

## 10. Architectural Risks

### Risk 1: The `Renderer.cpp` God Object — CRITICAL

**Evidence:** `Rendering/Core/Renderer.cpp` is 395 KB (~10,000+ lines). The `Renderer` class in `Renderer.h` (722 lines) exposes public member variables (`VkPipeline shadowPipeline`, `Camera camera`, `RenderSceneCache scene`), owns visibility buffers, post-process settings, shadow atlas, GPU profiling, editor overlays, and screen-space effects simultaneously. Its header has 43 `#include` directives.

**Impact:** Any change to any rendering feature risks breaking unrelated features. The class cannot be unit-tested. It cannot be replaced incrementally. It is a blocker for the layered architecture migration.

### Risk 2: Two Parallel ECS Implementations

**Evidence:**
- `ECS/Coordinator.h` — archetype-style with `Signature` bitsets, `ComponentManager<>`, `SystemManager`
- `Serializer/ECS/ECS.h` — schema-driven serialization-aware ECS with `SchemaRegistry`, `SerializationBridge`

Both are compiled and linked. It is unclear which is the canonical ECS. The `EngineRuntime` uses `IECSWorld` interface but the concrete implementation used at runtime must be determined by inspection of `EngineRuntime.cpp`.

### Risk 3: Two Rendering Backends

**Evidence:**
- `RenderingEngine/Renderer/` — legacy SceneRenderer, Passes, Techniques (all mostly empty .cpp files)
- `Rendering/Core/Renderer.cpp` — active, monolithic renderer

The `RenderingEngine` CMake library target includes BOTH `RenderingEngine/Runtime/engine/EngineLoop.cpp` and `Rendering/Core/Renderer.cpp`. The legacy renderer files are still in the CMake source list but have empty implementations. This creates confusion and potential linker issues.

### Risk 4: Test Code In Production Library

**Evidence (from CMakeLists.txt):** `EngineRuntime` target explicitly includes:
```
Runtime/Private/FormatTests.cpp      (112 KB)
Runtime/Private/AssetCacheTests.cpp  (16 KB)
Runtime/Private/GPUSceneTests.cpp    (15 KB)
Core/Diagnostics/StressTest.cpp      (125 KB)
Runtime/Private/AssetLoadingStressTests.cpp
Runtime/Private/PackageTests.cpp
...many more
```
All tests are compiled into the production `EngineRuntime` library. This inflates binary size, compilation time, and couples test code with shipping code.

### Risk 5: Namespace Fragmentation

**Evidence:** The codebase uses five different namespaces for related code:
- `eng::runtime` — runtime systems
- `eng::renderer` — rendering systems
- `eng::physics` — physics
- `Omnix::Radiance` — GI / radiance settings
- `Omnix` — EventManager, WorldManager
- Global namespace — ECS components, Scene classes, legacy Renderer classes

This creates include order dependencies and naming collisions.

### Risk 6: RHI Abstraction Never Implemented

**Evidence:** Both `Rendering/RHI/` (6 tiny headers) and `RenderingEngine/rhi/` (13 stub headers) define RHI interfaces that have zero implementation files. All actual GPU calls go through raw Vulkan + VMA. This means the engine is permanently locked to Vulkan with no viable path to DX12, Metal, or WebGPU without rewriting the entire renderer.

### Risk 7: Editor Always Compiled Into Runtime

**Evidence:** `EngineRuntime` CMake target includes all editor .cpp files. There is no conditional compilation flag to produce a pure runtime binary without the editor.

---

## 11. Technical Debt

### 11.1 God Objects

| Class | Location | Size | Problem |
|---|---|---|---|
| `Renderer` | `Rendering/Core/Renderer.cpp` | 395 KB | Owns entire rendering pipeline |
| `EditorLayer` | `Runtime/Private/Editor/EditorLayer.cpp` | 167 KB | Owns entire editor |
| `ViewportPanel` | `Runtime/Private/Editor/Panels/ViewportPanel.cpp` | 77 KB | Too many responsibilities |
| `ComponentWidgets` | `Runtime/Private/Editor/Widgets/ComponentWidgets.cpp` | 63 KB | All component UI in one file |
| `InspectorPanel` | `Runtime/Private/Editor/Panels/InspectorPanel.cpp` | 51 KB | All inspection logic in one file |
| `EngineRuntime` | `Runtime/Private/EngineRuntime.cpp` | 36 KB | God initializer for all subsystems |
| `FormatTests` | `Runtime/Private/FormatTests.cpp` | 112 KB | Should be a separate test executable |
| `StressTest` | `Core/Diagnostics/StressTest.cpp` | 125 KB | Should be a separate test executable |

### 11.2 Overly Coupled Systems

- `ECSComponents.h` imports `Runtime/Public/Gameplay/*` — ECS layer knows about gameplay
- `Renderer.cpp` imports `EditorViewportRenderer.h` — renderer is editor-aware
- `EngineRuntime` includes editor, gameplay, audio, tests all in one target
- Physics directly includes `ECS/Coordinator.h`

### 11.3 Duplicate Implementations

| Feature | Location 1 | Location 2 |
|---|---|---|
| Matrix4x4 | `Scene/Matrix4x4.h` | `ECS/Matrix4x4.h` |
| Transform | `Scene/Transform.h` | `RenderingEngine/Runtime/World/Transform.h` |
| World | `Core/World.h` | `RenderingEngine/Runtime/World/World.h` |
| ECS | `ECS/Coordinator.h` | `Serializer/ECS/ECS.h` |
| RHI | `Rendering/RHI/` | `RenderingEngine/rhi/` |
| RenderGraph | `Rendering/Graph/RenderGraph.cpp` | `RenderingEngine/Runtime/frameGraph/` |
| Logging | `Core/Logger.h` | `RenderingEngine/Core/Engine/Log.h` |
| Timer | `Core/Timer.h` | `RenderingEngine/Core/Engine/Timer.h` |
| Frustum / Visibility | `Rendering/Visibility/Frustum.h` | `RenderingEngine/Runtime/Visibility/Frustum.h` |
| Quaternion | `Scene/Quaternion.h` + `Scene/Quaterion.h` (typo) | Two files |

### 11.4 Dead Code / Empty Implementations

All files in `RenderingEngine/Renderer/Techniques/` and `RenderingEngine/Renderer/Passes/` are zero-byte `.cpp` files. They represent the old rendering architecture that has been abandoned in favor of `Renderer.cpp` but are still present in the build target.

### 11.5 Hardcoded Project Logic

- `VerticalSliceGameMode` — A specific game mode for one project, compiled into the engine's runtime library
- `ObjectivesSystem`, `CheckpointSystem` — Game-specific features compiled into the engine core
- `RADIANCE_V0_1_AUDIT.md`, `v0.3_VALIDATION_RESULTS.md` at root — Development artefacts in the repository root

### 11.6 Architectural Violations

- **Editor in Runtime:** `Runtime/Private/Editor/` is part of `EngineRuntime` CMake target
- **Gameplay in Runtime:** `Runtime/Private/Gameplay/` is part of `EngineRuntime` CMake target
- **Tests in Runtime:** Multiple test `.cpp` files are part of `EngineRuntime` CMake target
- **Singleton Abuse:** `EnvironmentSystem::Get()`, `RVGPageStreamingManager::Get()`, `ReflectionRegistry::Get()` — all global singletons that make testing and modular reuse difficult

### 11.7 Naming / Spelling Issues

- `Scene/Quaterion.h` — misspelling of Quaternion (two files: `Quaternion.h` and `Quaterion.h` both present)
- `RenderingEngine/Renderer/Techniques/DefferedRenderer.cpp` — "Deffered" misspelling
- `Components/Physical/AngularVelocity,h` — comma instead of period in filename

---

## 12. Subsystem Readiness Matrix

| Subsystem | Level | Reasoning |
|---|---|---|
| Logger | 3 — Stable | Full implementation, file output, categories |
| Linear/Pool/Stack Allocators | 3 — Stable | Implemented, tested |
| FreeList Allocator | 3 — Stable | Implemented; validation suite |
| VulkanInstance/Device | 3 — Stable | Working Vulkan bootstrap |
| VulkanSwapchain | 3 — Stable | Working swapchain + resize |
| EngineResources | 3 — Stable | Central Vulkan aggregator |
| GPUScene | 3 — Stable | Stable handles, well-designed |
| FrustumCullPass | 3 — Stable | GPU compute, readback |
| HZBPass | 3 — Stable | |
| OcclusionCullPass | 3 — Stable | |
| IndirectCommandBuildPass | 3 — Stable | |
| GeometryArena | 3 — Stable | Pool-based, stats |
| RVGCooker | 3 — Stable | Full pipeline |
| CapabilityTiers | 3 — Stable | Hardware detection |
| Renderer (as a whole) | 2 — Functional | Works but God Object |
| RenderTargetManager | 3 — Stable | |
| Scene / SceneManager | 3 — Stable | JSON load/save |
| ECS (Coordinator style) | 3 — Stable | |
| Physics (PhysX) | 3 — Stable | Real PhysX integration |
| PhysicsDebugDraw | 2 — Functional | 37 KB impl; works |
| Asset Registry | 3 — Stable | UUID-keyed metadata |
| Asset Manager | 3 — Stable | Load/cache lifecycle |
| GLTF/OBJ/Mesh Importers | 3 — Stable | Full import pipeline |
| Texture Importer | 3 — Stable | |
| Binary Format (Mesh/Mat/Tex) | 3 — Stable | Checksum-validated |
| Hot Reload | 2 — Functional | System exists; partial integration |
| Package System | 2 — Functional | Mount/unmount working |
| Serialization (Binary/Text) | 3 — Stable | Round-trip tested |
| Delta Serialization | 2 — Functional | |
| Reflection | 2 — Functional | Macro-based; no codegen |
| Input System | 2 — Functional | Win32; keyboard/mouse |
| Window System | 3 — Stable | Win32 + GLFW |
| Editor (overall) | 3 — Stable | Functional editor |
| ViewportPanel | 3 — Stable | 77 KB; offscreen rendering |
| InspectorPanel | 3 — Stable | Full component editing |
| SceneHierarchyPanel | 3 — Stable | |
| EditorCamera | 3 — Stable | |
| Gizmos (ImGuizmo) | 3 — Stable | |
| ConsolePanel | 1 — Prototype | 395 bytes |
| Audio (miniaudio) | 2 — Functional | Basic playback works |
| Event System | 2 — Functional | |
| CVarSystem | 2 — Functional | |
| ConfigSystem | 2 — Functional | |
| RuntimeConsole | 2 — Functional | |
| PluginManager | 2 — Functional | DLL loading |
| ModuleManager | 2 — Functional | |
| WorldManager | 2 — Functional | Multi-zone |
| GameMode | 2 — Functional | Abstract base |
| CheckpointSystem | 2 — Functional | |
| GameplaySaveSystem | 2 — Functional | |
| InteractionSystem | 2 — Functional | |
| RVGPageStreamingManager | 2 — Functional | Background thread |
| EnvironmentSystem | 2 — Functional | HDR loading |
| RenderGraph | 2 — Functional | 28 KB; used in Renderer |
| ECS (Serializer/ECS) | 2 — Functional | Schema partially implemented |
| MaterialSystem | 1 — Prototype | 526-byte stub |
| DeferredLightingPass | 1 — Prototype | Stub class |
| ShadowRenderer | 1 — Prototype | Stub class |
| TonemapPass | 1 — Prototype | Stub class |
| GPUProfiler | 1 — Prototype | Stub class |
| Animation | 0 — Concept | Only component struct defined |
| Navigation | 0 — Concept | Zero-byte header |
| Networking | 0 — Concept | Not started |
| Scripting | 0 — Concept | Component defined; no runtime |
| RHI Abstraction | 0 — Concept | Headers only; no implementation |
| FrameGraph (new) | 0 — Concept | Headers only; zero-byte impls |

---

## 13. Feature Completeness Matrix

| Feature | Complete | Partial | Missing |
|---|---|---|---|
| Deferred Rendering Pipeline | ✅ | | |
| PBR Shading | ✅ | | |
| Cascaded Shadow Maps | ✅ | | |
| GPU Frustum Culling | ✅ | | |
| HZB Occlusion Culling | ✅ | | |
| GPU Indirect Draw | ✅ | | |
| Visibility Buffer (Nanite-like) | | ✅ | |
| RVG Geometry Streaming | | ✅ | |
| Tonemapping / Post-Process | ✅ | | |
| SSAO | | ✅ | Disabled by default |
| SSR | | ✅ | Compiled; integration depth unknown |
| TAA | | ✅ | Compiled; integration depth unknown |
| Depth of Field | | ✅ | |
| Environment / IBL | | ✅ | HDR loading; probe baking unclear |
| Clustered Lighting | | ✅ | |
| Transparent Pass | ✅ | | |
| Shadow Atlas (Local Lights) | | ✅ | |
| Debug Draw | | ✅ | |
| Scene Editor (Viewport) | ✅ | | |
| Hierarchy Panel | ✅ | | |
| Inspector / Component Editing | ✅ | | |
| Asset Browser | | ✅ | |
| ImGuizmo Gizmos | ✅ | | |
| Asset Import (GLTF/OBJ) | ✅ | | |
| Hot Reload | | ✅ | |
| Binary Asset Formats | ✅ | | |
| Package System | | ✅ | |
| PhysX Integration | ✅ | | |
| Collision Primitives | ✅ | | |
| Trigger System | ✅ | | |
| Audio Playback | | ✅ | |
| 3D Spatial Audio | | | ✅ |
| ECS (Classic) | ✅ | | |
| Serialization | ✅ | | |
| Reflection | | ✅ | |
| Memory Allocators | ✅ | | |
| Logging | ✅ | | |
| Input (KB/Mouse) | ✅ | | |
| Gamepad Input | | ✅ | |
| Win32 Window | ✅ | | |
| Save/Load (Gameplay) | | ✅ | |
| Plugin System | | ✅ | |
| CVars | | ✅ | |
| Configuration | | ✅ | |
| World/Zone System | | ✅ | |
| Prefab System | | ✅ | |
| Checkpoint System | | ✅ | |
| Animation Runtime | | | ✅ |
| Skeletal Mesh | | | ✅ |
| Navigation / NavMesh | | | ✅ |
| Scripting | | | ✅ |
| Networking | | | ✅ |
| Terrain | | | ✅ |
| Particles | | | ✅ |
| Material Editor | | | ✅ |
| Animation Editor | | | ✅ |
| Profiler Panel | | | ✅ |
| Cross-Platform | | | ✅ |
| Standalone Runtime | | | ✅ |
| Distribution Pipeline | | | ✅ |

---

## 14. Recommended Layer Migration Plan

### Target Architecture (Proposed)

```
┌─────────────────────────────────────────────────────┐
│                  PRODUCT LAYER                       │
│  (VerticalSliceGameMode, HUD, project-specific code) │
├─────────────────────────────────────────────────────┤
│               GAME BACKEND LAYER                     │
│  (GameMode, CheckpointSystem, InteractionSystem,     │
│   GameplaySaveSystem, ObjectiveSystem, WorldManager, │
│   AudioSystem, GameplayEventBus)                     │
├─────────────────────────────────────────────────────┤
│                 EDITOR LAYER                         │
│  (EditorLayer, all Panels, ComponentWidgets,         │
│   EditorCamera, ImGuizmo, EditorViewportRenderer,    │
│   AssetImportService, EditorCommands)                │
├─────────────────────────────────────────────────────┤
│               RUNTIME LAYER                          │
│  (EngineRuntime, AssetManager, AssetRegistry,        │
│   HotReloadSystem, PackageManager, PluginManager,    │
│   ModuleManager, ConfigSystem, CVarSystem,           │
│   RuntimeConsole, TimeManager, UUIDSystem,           │
│   EventBus, ServiceRegistry)                         │
├─────────────────────────────────────────────────────┤
│               KERNEL LAYER                           │
│  (Rendering Pipeline, GPUScene, Visibility Passes,   │
│   GeometryArena, RVGStreaming, PhysX, Scene, ECS,    │
│   Serialization, Reflection, Memory, Logger,          │
│   Input, Window, Vulkan, EngineResources)            │
└─────────────────────────────────────────────────────┘
```

### Migration Table

| Subsystem | Current Location | Recommended Move | Reason |
|---|---|---|---|
| Logger | `Core/Logging/` | **Remain in Kernel** | Already correct |
| Memory Allocators | `Core/Memory/` | **Remain in Kernel** | Already correct |
| Vulkan (Instance/Device/Swapchain) | `RenderingEngine/Vulkan/` | **Remain in Kernel** | Core GPU abstraction |
| EngineResources | `RenderingEngine/Core/Engine/` | **Remain in Kernel** | Central Vulkan state |
| GPUScene | `Rendering/GPUScene/` | **Remain in Kernel** | GPU data layer |
| Visibility Passes | `Rendering/Visibility/` | **Remain in Kernel** | GPU compute |
| GeometryArena | `Rendering/Geometry/Arena/` | **Remain in Kernel** | GPU memory |
| RVGPageStreamingManager | `Rendering/Geometry/Streaming/` | **Remain in Kernel** | GPU streaming |
| RenderGraph | `Rendering/Graph/` | **Remain in Kernel** | Core pass scheduling |
| Renderer (God Object) | `Rendering/Core/Renderer.cpp` | **Split into multiple modules** | Deferred, Shadows, PostProcess, Overlay, Visibility — each becomes its own module |
| MaterialSystem | `Rendering/Materials/` | **Remain in Kernel** (after implementation) | Core GPU resource |
| PhysicsWorld | `Physics/Private/` | **Remain in Kernel** | Core simulation |
| ECS (Coordinator) | `ECS/` | **Remain in Kernel** | Already correct |
| ECS (Serializer/ECS) | `Serializer/ECS/` | **Merge with Kernel ECS** | Redundant — schema should be merged into single ECS |
| Scene / SceneManager | `Scene/` | **Remain in Kernel** | Core world state |
| Serialization | `Serializer/Serialization/` | **Remain in Kernel** | Core data layer |
| Reflection | `Runtime/Public/Reflection.h` | **Move to Kernel** | Should not be in Runtime |
| Input | `Input/`, `RenderingEngine/Platform/Input/` | **Remain in Kernel** | Core input |
| Window | `RenderingEngine/Platform/window/` | **Remain in Kernel** | Platform abstraction |
| AssetRegistry / AssetManager | `Runtime/Private/` | **Remain in Runtime** | Correct placement |
| HotReloadSystem | `Runtime/Private/` | **Remain in Runtime** | Correct |
| PackageManager | `Runtime/Private/` | **Remain in Runtime** | Correct |
| PluginManager | `Runtime/Private/` | **Remain in Runtime** | Correct |
| ConfigSystem | `Runtime/Private/` | **Remain in Runtime** | Correct |
| CVarSystem | `Runtime/Private/` | **Remain in Runtime** | Correct |
| RuntimeConsole | `Runtime/Private/` | **Remain in Runtime** | Correct |
| EditorLayer + all Panels | `Runtime/Private/Editor/` | **Move to Editor Layer** | Must be separated from runtime |
| Rendering/Editor/ (passes) | `Rendering/Editor/` | **Move to Editor Layer** | Editor-specific rendering |
| GameMode | `Runtime/Private/Gameplay/` | **Move to Game Backend** | Not engine-core |
| CheckpointSystem | `Runtime/Private/Gameplay/` | **Move to Game Backend** | Not engine-core |
| InteractionSystem | `Runtime/Private/Gameplay/` | **Move to Game Backend** | |
| GameplaySaveSystem | `Runtime/Private/Gameplay/` | **Move to Game Backend** | |
| ObjectiveSystem | `Runtime/Private/Gameplay/` | **Move to Game Backend** | |
| VerticalSliceGameMode | `Runtime/Private/Gameplay/` | **Move to Product Layer** | Project-specific |
| GameplayHUD | `Runtime/Private/Gameplay/UI/` | **Move to Product Layer** | Project-specific |
| StateObjects (Door, etc.) | `Runtime/Public/Gameplay/StateObjects/` | **Move to Product Layer** | Project-specific |
| AudioSystem | `Runtime/Private/Audio/` | **Move to Game Backend** | |
| WorldManager | `Runtime/Private/World/` | **Move to Game Backend** | |
| Test files (`*Tests.cpp`) | Various | **Move to dedicated Tests target** | Must exit production code |
| StressTest.cpp | `Core/Diagnostics/` | **Move to dedicated Tests target** | |
| FormatTests.cpp | `Runtime/Private/` | **Move to dedicated Tests target** | |
| RVGCooker | `Tools/RVGCooker/` | **Remain as Offline Tool** | Already separate executable |
| Components (Behavior stub headers) | `Components/Behavior/` | **Move to Game Backend stubs** | Game-layer concepts |

---

## 15. Critical Issues

1. **`Renderer.cpp` (395 KB) is a single-point-of-failure God Object.** Every rendering feature is intertangled. This class must be decomposed before further features can be added safely. Estimated decomposition: 6–10 smaller classes (GBufferRenderer, ShadowMapRenderer, DeferredLightingRenderer, PostProcessRenderer, VisibilityRenderer, DebugRenderer, EditorOverlayRenderer, etc.).

2. **All editor code compiles into the production runtime.** There is no way to produce a standalone game binary without the editor. This must be fixed by separating the `EngineRuntime` CMake target into `EngineRuntime` (kernel+runtime) and `EditorRuntime` (adds editor on top).

3. **Test code (`FormatTests.cpp` 112 KB, `StressTest.cpp` 125 KB) is part of the production library.** These are the largest files in the entire codebase and serve no purpose in a shipped build. They must be extracted to a dedicated test executable target.

4. **Two ECS implementations — one must be chosen as canonical.** The `ECS/Coordinator.h` (classic) and `Serializer/ECS/ECS.h` (schema-aware) are both compiled. The schema-aware one should either subsume or extend the classic one.

5. **`DeferredLightingPass`, `ShadowRenderer`, `TonemapPass`, `ExposurePass`, `SkyLightRenderer` are all stubs.** The actual lighting code lives inside `Renderer.cpp`. This is a planning mismatch — the new rendering architecture is described by these headers but not yet migrated.

6. **No RHI abstraction has any implementation.** The engine will remain permanently Vulkan-locked at the architectural level unless either the RHI headers are implemented or the abstraction layer is abandoned and documented as Vulkan-only.

7. **`EngineRuntime` CMake library target includes gameplay, editor, test, and audio code.** All belong in separate CMake library targets.

8. **The `Omnix.log` file is 492 MB.** This indicates the engine is running in `Trace` log level during development. This should be reduced to `Info` or `Debug` for normal development and `Off` in production builds.

---

## 16. Suggested Immediate Priorities

_(0–4 weeks — should not require architectural redesign)_

1. **Separate tests from production library.** Create a dedicated `EngineTests` CMake executable target and move all `*Tests.cpp` files and `StressTest.cpp`/`FormatTests.cpp` to it. This immediately improves compilation time and production binary size.

2. **Add a compile-time `OMNIX_EDITOR` flag.** Wrap all `Runtime/Private/Editor/` includes in `#ifdef OMNIX_EDITOR`. This enables building a standalone runtime binary without the editor being compiled in.

3. **Reduce default log level.** Change `LogLevel::Trace` in `main()` to `LogLevel::Info`. Add `--trace` flag for verbose sessions. The current 492 MB log file is a clear indicator.

4. **Fix the `Quaterion.h` typo and `.h` filename with comma.** Two trivial filesystem inconsistencies (`Scene/Quaterion.h` duplicate, `Components/Physical/AngularVelocity,h`) that could cause build issues on case-sensitive filesystems (Linux).

5. **Document which ECS is canonical.** Add a `README` in both `ECS/` and `Serializer/ECS/` folders clarifying whether they are parallel systems or one supersedes the other. This is foundational for the layer migration plan.

6. **Activate CI test execution.** Extend the GitHub Actions workflow to actually run `sampler` and `transform_tests` executables as a basic smoke test.

---

## 17. Suggested Medium-Term Priorities

_(1–3 months — requires architectural decisions)_

1. **Decompose `Renderer.cpp` into focused subsystem classes.** Prioritize extracting:
   - `DeferredLightingRenderer` (fulfill the existing stub)
   - `ShadowMapRenderer` (fulfill the existing stub)
   - `PostProcessRenderer` (fulfill the existing stub)
   The stubs already define the interfaces; the code just needs to be migrated.

2. **Separate `EngineRuntime` CMake target.** Create `GameBackend` and `EditorRuntime` targets. This enforces the layered architecture at the build level.

3. **Implement the canonical RHI or explicitly abandon it.** Either implement `Rendering/RHI/` with Vulkan backends (inheriting from `EngineResources`), or remove the stub headers and document "Vulkan-only by design" to prevent confusion.

4. **Implement `MaterialSystem`.** The current 526-byte stub means materials are handled ad-hoc inside `Renderer.cpp`. A proper material graph with parameter binding, variant caching, and hot-reload is critical for production content.

5. **Implement `GPUProfiler`** to provide GPU timing data. This is essential for performance work.

6. **Implement Animation Runtime.** The `OmnixAnimFormat.h` and `OmnixSkinnedVertex` struct are defined. The runtime evaluator, joint matrix computation, and skinning shader are all missing.

7. **Implement Scripting Runtime.** The `ScriptComponent` has no backing runtime. Consider Lua or a custom scripting bridge for gameplay logic.

8. **Create a `ConsolePanel` implementation.** The current 395-byte file provides nothing. This is the primary developer debugging surface.

---

## 18. Suggested Long-Term Priorities

_(3–12 months — large features)_

1. **Full RHI implementation with DX12 backend.** Build out the `Rendering/RHI/` layer with Vulkan and DirectX 12 concrete backends. This enables console and Xbox support.

2. **Navigation / NavMesh System.** Currently zero implementation. Required for any game with AI movement. Consider integrating Recast/Detour.

3. **Particle System.** No GPU particle system exists. Required for visual effects.

4. **Terrain System.** No terrain rendering. Required for open-world games.

5. **Audio Spatialization (3D Audio).** Extend the miniaudio integration with 3D positional audio, reverb zones, and obstruction.

6. **Networking.** No implementation. Required for multiplayer features. Architecture-level decision needed (lockstep vs. client-server).

7. **Material Editor Panel.** A visual node-based material editor in the editor layer.

8. **Animation Editor Panel.** Curve editing, state machine visualization, blend tree authoring.

9. **Cross-Platform Window/Input Backend.** Generalize `WindowWin32.h` and `InputWin32.cpp` behind a platform abstraction to enable Linux/Mac builds.

10. **Full Packaging and Distribution Pipeline.** Asset cooking, content bundling, executable packaging, update system.

11. **Performance Profiler Panel.** GPU timeline, CPU flamegraph, memory tracker — integrated into the editor.

---

## 19. Overall Production Readiness Score

| Layer | Score | Notes |
|---|---|---|
| Rendering Pipeline | 3.5 / 5 | GPU-driven pipeline is functionally impressive for v0.4 |
| Editor | 3.0 / 5 | Functional but monolithic |
| Asset System | 3.0 / 5 | Complete pipeline exists |
| Physics | 3.0 / 5 | Real PhysX working |
| Scene / ECS | 3.0 / 5 | Two ECS systems; functional |
| Audio | 2.0 / 5 | Basic playback only |
| Gameplay | 2.0 / 5 | Vertical-slice quality, not general |
| Memory | 3.0 / 5 | Multiple allocators; no unified strategy |
| Serialization | 3.0 / 5 | Binary formats; delta system |
| Threading / Jobs | 1.0 / 5 | Mostly stubs |
| Networking | 0.0 / 5 | Not started |
| Animation | 0.5 / 5 | Only format spec exists |
| Testing / CI | 1.5 / 5 | Tests exist but untriggered |
| Architecture / Cleanliness | 1.5 / 5 | Major coupling, God Objects |
| **Weighted Overall** | **2.7 / 5** | Functional–Stable boundary |

---

## 20. Overall Engine Maturity Assessment

**OmnixEngine v0.4 is a technically impressive, actively-developed custom engine that has achieved a sophisticated rendering capability that exceeds many academic or hobbyist engines of its development vintage.**

**Strengths:**
- GPU-driven indirect rendering with frustum, HZB, and occlusion culling is rarely seen outside commercial engines at this stage
- The RVG (Renderable Virtual Geometry) system with its offline cooker, runtime page streaming, and cluster-level GPU culling demonstrates serious rendering engineering ambition
- The asset system with binary formats, checksums, hot-reload, and packaging is production-grade in design
- The editor has all of the core panels expected of a game engine editor
- The PhysX integration is real and functional, not mocked
- The test suite is extensive (multiple large stress test files)

**Weaknesses:**
- The `Renderer.cpp` God Object (395 KB) is the primary architectural liability and represents significant accumulated technical debt
- Editor code is architecturally inseparable from the runtime, blocking standalone runtime builds
- Test code is compiled into production binaries
- Two parallel ECS and RHI systems create confusion and dead weight
- Major feature gaps (animation, navigation, scripting, networking, particles) block game production use
- Win32-only build system prevents any cross-platform deployment

**Maturity Classification:**  
> **Technology Demonstrator / Early Development Engine**  
> The rendering technology is impressive and demonstrates clear long-term vision. The engine is suitable for a single experienced developer or small team to build a vertical slice demonstration game. It is not yet suitable for onboarding content creators (no material editor, no animation editor), for multi-developer teams (no clean layering, no stable API contracts), or for commercial product distribution (no packaging pipeline, editor always compiled in).

**Estimated Time to Production-Ready (Level 5) given current trajectory:**
- With architectural cleanup prioritized: **12–18 months**
- Without architectural cleanup: **24–36 months** (technical debt compounds)

---

*Report generated by Antigravity Architecture Review Agent.*  
*All findings reference concrete file evidence from `d:\OmnixEngine`.*  
*No source code was modified during this audit.*
