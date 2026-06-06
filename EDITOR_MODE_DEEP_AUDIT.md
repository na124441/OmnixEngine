# 🌌 Omnix Editor Mode: Deep Technical & UX Audit (v0.4)

This audit evaluates the **Editor Mode** of the **Omnix Studio Engine v0.4** from both a system-level C++ perspective and an end-user level design UX perspective. The report is based strictly on the current codebase under `d:\OmnixEngine`, highlighting major design flaws, system bottlenecks, and architectural limitations that prevent it from being a practical level creation tool.

---

## 1. Editor Launch and Boot Flow

### Status
`PARTIAL`

### Evidence
* [main.cpp](file:///d:/OmnixEngine/main.cpp#L20-L30) (Central lifecycle loop instantiation)
* [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L119-L121) (Command-line flag parsing)
* [Runtime/Private/EngineRuntime.cpp](file:///d:/OmnixEngine/Runtime/Private/EngineRuntime.cpp#L320-L330) (Editor instantiation & context seeding)
* [Runtime/Private/Editor/EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp#L75-L239) (`EditorLayer::Initialize` sequence)

### Current Behavior
* **CLI Flag Parsing**: The editor is triggered by passing `--editor` to the entry executable. In `EngineRuntime::Initialize`, the presence of this flag sets `m_Context.mode = RuntimeMode::Editor` (line 120).
* **Editor Layer Lifecycle**: If `RuntimeMode::Editor` is active, the engine instantiates the `EditorLayer` via a `std::unique_ptr` and registers it in the startup system tracker.
* **Separation from Standalone**: Physically, the editor is built into the same executable target (`Application`) as the standalone runtime. Logically, it is separated using condition gating based on `m_Context.mode`.
* **Initialization Order**: The `EditorLayer` is initialized *after* the graphics loop (`EngineLoop`).
* **State Machine**: The editor runs its own simulation state using the enum `EditorSimulationState` (Edit, Play, Pause).
* **Default Scene Loading**: On boot, the editor does *not* load any default scene. It starts with a completely empty world, forcing the designer to manually select File -> Load Scene.
* **Layout Persistence**: Window positions and docking structures are saved to and loaded from `Config/Editor/imgui.ini` (which is configured in `EditorLayer::Initialize` at line 103).

### Problems
1. **Unsafe Initialization Ordering (Crash Vector)**: Because `EditorLayer::Initialize` runs after `EngineLoop::Initialize`, if any Vulkan subsystem fails (e.g. swapchain recreation, descriptor pool creation), the engine attempts to run `Shutdown()` immediately. However, `Shutdown()` will attempt to free `m_Editor` resources that were never successfully constructed, resulting in a null-pointer dereference or an assertion crash.
2. **Missing Shaders/Tools Crash**: If the SPIR-V shader files (`vert.spv`, `frag.spv`, and shaders under `/shaders`) are missing or fail to compile because the Vulkan compiler (`glslc`) is not present in the environment's `PATH`, the rendering engine crashes during initialization, preventing the editor from launching at all.
3. **No Scene on Launch (Bad UX)**: Starting with a completely empty layout instead of auto-loading a default templates scene or the last-modified level feels unpolished. The editor displays a black screen in the viewport until the user manually types or selects a JSON file.
4. **Hardcoded Viewport Sizes**: Although ImGui layouts persist via `imgui.ini`, the viewport dimensions internally reset to hardcoded `1280.0f` and `720.0f` on startup. The offscreen framebuffer is only resized *after* the viewport panel is rendered for the first time, causing a jarring redraw/recreation pass on the first frame.

### Required Fixes
1. **Decouple Initialization Paths**: Wrap `EditorLayer::Initialize` inside a try-catch block and ensure that `Shutdown` only releases resources that have a valid non-null handle.
2. **Auto-Load Last Session Scene**: Implement a tiny editor config file (`Config/Editor/editor_settings.json`) that writes the active scene path on shutdown and auto-loads it on boot.
3. **Boot/Setup Safeguards**: If Vulkan pipelines or shaders fail, show a fallback Win32 message box warning the developer of the missing compiler/Vulkan SDK instead of crashing silently or writing to `Omnix.log` without a window display.

---

## 2. Viewport and Camera Navigation

### Status
`PARTIAL`

### Evidence
* [Runtime/Private/Editor/Panels/ViewportPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/ViewportPanel.cpp)
* [Runtime/Private/Editor/EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp#L254-L327) (Viewport update and resizing)

### Current Behavior
* **Rendering Viewport**: Viewport panel rendering runs by displaying the offscreen Vulkan texture (`viewportTexture`) using `ImGui::Image`.
* **Editor Camera**: The editor implements an `EditorCamera` that updates its view coordinates based on WASD keys and Right Mouse Button (RMB) dragging.
* **Resizing Hook**: In `EditorLayer::Render()`, the editor checks if the viewport panel size has changed, calls `vkDeviceWaitIdle()`, resets command pools, and invokes `CreateOffscreenResources()` to match the new scale.
* **Diagnostics Overlay**: Renders viewport diagnostics at the bottom-left of the viewport panel when `m_ShowDiagnostics` is enabled.

### Problems
1. **Multi-Viewport Swapchain Crash**: In `EditorLayer::Initialize` (line 108), the flag `ImGuiConfigFlags_ViewportsEnable` is explicitly disabled:
   ```cpp
   io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable; // Disable Multi-Viewports to prevent Vulkan surface errors
   ```
   If a designer attempts to drag the Viewport window or any other panel outside the main window bounds, the application crashes or locks because the Vulkan backend lacks multi-window swapchain management. This restricts editing to a single screen dockspace.
2. **Resizing Bottleneck / Stutter**: Resizing the viewport window causes significant framerate drops because it blocks the CPU thread with `vkDeviceWaitIdle()` and destroys/recreates Vulkan images on the fly. Doing this during drag-resizes causes the editor window to stutter heavily.
3. **Lack of Viewport Gizmo Feedback**: The viewport uses **ImGuizmo** for editing translations/rotations, but key operations like coordinate snapping (grid snapping) or coordinate space toggling (World vs Local) are missing from the toolbar overlay, forcing the designer to do freehand translations.

### Required Fixes
1. **Vulkan Multi-Window Swapchain Support**: Rewrite the Vulkan swapchain wrapper to support secondary swapchains matching ImGui platform windows when dragging windows outside the master dockspace.
2. **Throttled Viewport Resizing**: Instead of recreating Vulkan render resources on every pixel change during a window drag-resize, throttle the recreation. Wait until the user has finished resizing (e.g. mouse release) or recreate resources only after a 100ms idle delay.

---

## 3. Scene Hierarchy and Node Management

### Status
`PARTIAL`

### Evidence
* [Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp#L67-L87)
* [Scene/SceneObject.cpp](file:///d:/OmnixEngine/Scene/SceneObject.cpp)

### Current Behavior
* **Flat Tree Rendering**: The hierarchy panel iterates through the ECS Coordinator's active entities list (`coordinator.GetActiveEntities()`) and renders each as a tree node:
  ```cpp
  const auto& activeEntities = coordinator.GetActiveEntities();
  for (Entity entity : activeEntities) {
      ...
      ImGuiTreeNodeFlags flags = ... | ImGuiTreeNodeFlags_Leaf;
      bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", entityName.c_str());
      ...
  }
  ```
* **Object Hierarchy**: The scene graph classes (`SceneObject`, `Transform`) support parent-child relationships, transform propagation, and dirty updates.

### Problems
1. **Completely Flat Hierarchy UI (Critical UX Defect)**: The hierarchy panel renders all entities as a flat list with the `ImGuiTreeNodeFlags_Leaf` flag enabled. Parent-child relationships established in the C++ scene graph are completely ignored in the editor UI. A designer cannot tell which meshes are parented to which transform nodes.
2. **No Drag-and-Drop Parenting**: In a standard level editor, parenting is achieved by dragging one entity node onto another. The hierarchy panel has no drag-and-drop targets, meaning it is impossible to organize scene hierarchies or attach colliders to mesh nodes visually inside the editor.
3. **No Multi-Selection**: The selection model only supports selecting a single entity ID at a time. Designing levels is extremely tedious because you cannot select, move, or delete groups of entities simultaneously.

### Required Fixes
1. **Hierarchical Tree Traversal**: Refactor `SceneHierarchyPanel::Render` to query the scene's root objects (`Scene::GetRootObjects()`) and recursively traverse child nodes using `SceneObject::GetChildren()`.
2. **Drag-and-Drop Parenting Integration**: Use ImGui's Drag and Drop API (`ImGui::BeginDragDropSource` and `ImGui::AcceptDragDropPayload`) to let developers reparent entities visually.

---

## 4. Inspector and Component Editing

### Status
`IMPLEMENTED`

### Evidence
* [Runtime/Private/Editor/Panels/InspectorPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/InspectorPanel.cpp)
* [Runtime/Private/Editor/Widgets/ComponentWidgets.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Widgets/ComponentWidgets.cpp)
* [Runtime/Private/Editor/Widgets/TransformWidget.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Widgets/TransformWidget.cpp)

### Current Behavior
* **Reflection widgets**: Inspector panel draws fields of components attached to the selected entity.
* **Adding Components**: Provides an "Add Component..." button showing a popup to register enums like `ColliderComponent`, `LightComponent`, or `RigidBodyComponent` on the fly.
* **Component Modifications**: Users edit coordinates via `DragFloat3` or modify enums via `Combo` widgets. Changes mark the scene as dirty.

### Problems
1. **No Undo/Redo System (Critical UX Defect)**: There is no command buffer or undo/redo stack for inspector modifications. If a designer accidentally mistypes a position or deletes a component, the action is permanent. The only way to restore the level is to reload the scene from disk, losing all unsaved work.
2. **Unsafe Physics Rebuilds**: Modifying transform fields or box collider extents triggers a physical actor rebuild in the PhysX scene:
   ```cpp
   if (TransformWidget::Draw(transComp, dirtyState)) {
       if (sig.test(coordinator.GetComponentType<StaticBodyComponent>()) && m_Context->physicsWorld) {
           m_Context->physicsWorld->RebuildStaticActor(coordinator, selectedEntity);
       }
   }
   ```
   These rebuilds destroy and recreate the PhysX actor instantly. Doing this dynamically during dragging operations causes frame stutters and can lead to physics simulation crashes if the actor properties (like scale or bounds) temporarily resolve to zero.
3. **Lack of Asset Registry Synchronization**: Modifying mesh or material handle variables is handled by typing raw integers (UUIDs) inside text fields or double-clicking assets in the browser. There is no search box to easily assign registered meshes/materials.

### Required Fixes
1. **Implement Command Pattern for Edits**: Create a history tracker (`EditorHistory`) that records property changes as undoable/redoable commands.
2. **Deferred Physics Updates**: Instead of rebuilding the PhysX actor immediately on every tiny float drag, queue the rebuild. Wait until the user has finished editing (e.g. mouse release) before updating the physics actor.

---

## 5. Asset Browser and Resource Importing

### Status
`IMPLEMENTED`

### Evidence
* [Runtime/Private/Editor/Panels/AssetBrowserPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/AssetBrowserPanel.cpp)
* [Runtime/Private/Editor/AssetImportService.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/AssetImportService.cpp)

### Current Behavior
* **Asset Browser List**: Displays registered resources in a list table (columns for Name, Type, Handle, Path).
* **Double-Click Assignment**: Double-clicking an asset in the table assigns it to the selected entity's RenderMesh or Material component.
* **Asset Import**: Invokes `AssetImportService::ImportModel` to copy and compile external raw OBJ models into optimized `.omnixmesh` files.

### Problems
1. **Table-Only UX (Poor Designer UX)**: The browser is a flat table rather than a visual folder explorer. Level designers expect folder-based navigation and visual icon grids showing thumbnail previews of materials and textures. Navigating through hundreds of assets in a flat table is extremely difficult.
2. **Missing Standalone Asset Compiler**: Raw imports are compiled by the editor. The offline command-line compiler (`omnix-cli`) is missing from the build. This means raw FBX/PNG files cannot be compiled automatically in the background or during packaging.
3. **No Drag-and-Drop from Browser to Viewport**: You cannot drag a mesh asset from the browser directly into the 3D viewport to spawn a model at that location. The user must click "Create Entity From Selected Asset" which spawns the entity at the camera's center, requiring manual repositioning.

### Required Fixes
1. **Icon Grid & Folder Tree View**: Refactor the asset browser UI to display a folder directory tree on the left and a grid view of assets on the right with icon previews.
2. **Viewport Drag-to-Spawn**: Implement drag-and-drop targets in the `ViewportPanel`. Dragging a mesh asset from the browser into the viewport should perform a raycast into the 3D scene and spawn the entity at the intersection coordinate.

---

## 6. Play Mode (PIE) State Transitions

### Status
`IMPLEMENTED`

### Evidence
* [Runtime/Private/Editor/EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp#L1920-L2180) (`EnterPlayMode` and `ExitPlayMode`)

### Current Behavior
* **Scene Cloning**: Toggling play mode deep-clones the active scene and active ECS Coordinator in-memory.
* **Simulation Swapping**: Swaps the current active ECS context with the cloned simulation ECS, allowing physics simulation and player controller updates to run without modifying the original edit level.
* **Auto-Floor / Spawn Injector**: If the scene lacks a floor collider or player start point, it auto-injects a static floor box and prints warnings.
* **Simulation Restoration**: Toggling stop deletes the simulated scene/ECS and swaps the backed-up edit scene and ECS back.

### Problems
1. **Cursor Capture UX Trap**: Entering play mode captures the cursor (`CursorMode::Disabled`) inside the viewport. However, the user must press `Escape` to release the mouse cursor to press the "Stop" toolbar button. There is no visual prompt explaining how to release the mouse cursor, making play mode feel like a trap for first-time users.
2. **No Frame Stepping / Pausing**: Although play mode can be paused, there is no way to step through simulation frames step-by-step to debug collisions or trigger overlaps.
3. **Validation Block Blockages**: If `GameplayValidator` finds any fatal error (e.g. missing transform or duplicate entity names), it completely blocks entering Play Mode. While validation is good, it should ideally offer an "Ignore and Play anyway" path for quick prototyping.

### Required Fixes
1. **Visual Cursor Release Prompt**: When the cursor is captured in Play mode, display a subtle overlay at the top of the viewport (e.g. `"Press ESC to exit cursor capture"`) to guide the designer.
2. **Frame Step Debugger**: Add a "Step Frame" button to the toolbar that ticks the engine simulation for exactly one fixed timestep (16.6ms) while paused.

---

## 7. Critical Deficiencies Summary

| Deficiency | Severity | Architectural Impact | UX Impact |
| :--- | :--- | :--- | :--- |
| **Flat Hierarchy Tree** | **CRITICAL** | Relational components (`Parent`/`Child`) are completely ignored visually. | Designers cannot organize levels or view spatial dependencies. |
| **No Multi-Viewport Docking** | **HIGH** | Vulkan swapchain backend asserts and crashes if windows are dragged out. | Restricts level editing to a single main application screen. |
| **No Undo / Redo Stack** | **CRITICAL** | Property changes and entity deletions are immediately destructive. | Designers risk losing hours of work with a single mistyped float. |
| **Table-Only Asset Browser** | **MEDIUM** | flat table list with no folder navigation or previews. | Browsing assets is extremely slow and tedious. |
| **No Viewport Drag-to-Spawn** | **MEDIUM** | Mesh assets must be manually spawned and translated from origin. | Spawns entities at cam center, slowing down scene dressing. |
| **Zero-Load Editor Boot** | **LOW** | Editor launches with a blank screen rather than loading the last scene. | Unpolished first-time user experience. |
