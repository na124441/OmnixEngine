# Editor Mode Problems Diagnostic Report

Date: 2026-06-07

This report documents the current editor-mode problems reported in Omnix Engine and maps them to code-level evidence, likely root causes, and recommended fixes.

## Reported Problems

- File menu actions do not work:
  - New Scene
  - Load/Open Scene
  - Save Scene
  - Save Scene As
  - Reload Scene
  - Validate Scene
- Edit menu shows no options.
- Top-bar Save and Load buttons do not work.
- Scene Hierarchy does not show entity data.
- 3D world has no proper lighting or color.
- Mouse and keyboard control is not reliable or polished.

## Executive Summary

The editor has the visual shell of a functional tool, but several editor systems are not fully connected. The main issues are:

- File actions are wired to ImGui text-entry popups instead of real file dialogs and often fail silently.
- The Edit menu is explicitly empty in code.
- Editor entity creation primarily creates ECS entities, while the Scene Hierarchy reads from the active `Scene` graph, causing data visibility mismatches.
- Lighting has fallback/default paths and scene-light paths, but scene-light usage depends on ECS light components and `LightCollectionSystem` availability.
- Camera and gameplay controls are driven mostly through ImGui mouse/key polling, without a robust editor/game input ownership model or reliable OS-level cursor capture.

## 1. File Menu Actions Do Not Work

### Code Evidence

File menu trigger lambdas are defined in:

- `Runtime/Private/Editor/EditorLayer.cpp:739`
- `Runtime/Private/Editor/EditorLayer.cpp:748`
- `Runtime/Private/Editor/EditorLayer.cpp:757`
- `Runtime/Private/Editor/EditorLayer.cpp:770`
- `Runtime/Private/Editor/EditorLayer.cpp:774`

File menu items call those triggers in:

- `Runtime/Private/Editor/EditorLayer.cpp:1123`

Scene operations are implemented in:

- `Scene/SceneManager.cpp:105` - `LoadScene`
- `Scene/SceneManager.cpp:120` - `Update`
- `Scene/SceneManager.cpp:160` - `ProcessLoading`
- `Scene/SceneManager.cpp:206` - `SwitchScene`
- `Scene/SceneManager.cpp:312` - `ReloadCurrentScene`
- `Scene/SceneManager.cpp:363` - `CreateNewScene`
- `Scene/SceneManager.cpp:373` - `SaveActiveScene`

### Diagnosis

The menu items exist, but they do not directly perform visible file operations. They mainly open ImGui modal popups:

- `New Scene Name`
- `Open Scene`
- `Save Scene As`
- `Unsaved Changes`

These popups require manually typed paths. There is no real open/save file dialog used for the File menu path, even though a platform open dialog exists in `Runtime/Private/Editor/PlatformFileDialog.cpp`.

Several actions can also silently no-op:

- `Save Scene` only works if `sceneMgr && sceneMgr->GetActiveScene()` is true.
- `Validate Scene` only works if the active scene has a non-empty file path.
- `Reload Scene` only works if the active scene has a file path.
- `LoadScene()` only sets the scene manager state to `Loading`; actual load occurs later during `SceneManager::Update()`.
- `ProcessLoading()` validates the scene before loading it. If validation fails, loading is cancelled.

### User-Visible Effect

The user clicks menu items or top-bar Save/Load buttons and either sees no action, a confusing path popup, or a silent failure.

### Recommended Fix

- Replace text-only path popups with native file dialogs.
- Add a save file dialog equivalent to `PlatformFileDialog::ShowOpenDialog`.
- Show visible editor notifications for all failed or blocked actions.
- If `Save Scene` is clicked with no active scene, either create a default scene or show a clear error.
- If `Validate Scene` is clicked on an unsaved scene, run validation against in-memory scene data instead of requiring a file path.
- After `LoadScene()`, surface validation/load failure results in an editor modal or console panel.

## 2. Top-Bar Save And Load Buttons Do Not Work

### Code Evidence

Top-bar buttons are implemented in:

- `Runtime/Private/Editor/EditorLayer.cpp:898`
- `Runtime/Private/Editor/EditorLayer.cpp:904`
- `Runtime/Private/Editor/EditorLayer.cpp:908`

They call:

- `triggerSaveScene()`
- `triggerOpenScene()`

### Diagnosis

The top-bar Save and Load buttons share the same weak behavior as the File menu:

- Save does nothing if no active scene exists.
- Load opens a manual path popup instead of a file picker.
- Failures are not shown clearly in the editor UI.

### Recommended Fix

- Route top-bar Save and Load through the same improved file-action service used by the File menu.
- Add status toasts or console messages for success/failure.
- Disable buttons only when invalid, and explain why through tooltip text.

## 3. Edit Menu Has No Options

### Code Evidence

The Edit menu is explicitly empty:

- `Runtime/Private/Editor/EditorLayer.cpp:1157`

Current implementation:

```cpp
if (ImGui::BeginMenu("Edit")) { ImGui::EndMenu(); }
```

### Diagnosis

This is not a runtime bug. The menu has no implemented items.

### Recommended Fix

Add expected editor commands:

- Undo
- Redo
- Cut
- Copy
- Paste
- Duplicate
- Delete
- Rename
- Select All
- Deselect
- Frame Selected
- Editor Preferences

If undo/redo is not implemented yet, show disabled menu items instead of an empty menu.

## 4. Scene Hierarchy Does Not Show Entity Data

### Code Evidence

Scene Hierarchy renders from the active scene graph:

- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:192`

Editor create actions call ECS entity commands:

- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:134`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:138`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:237`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:240`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:244`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:247`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:250`
- `Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp:253`

Editor entity commands are implemented in:

- `Runtime/Private/Editor/Commands/EditorEntityCommands.cpp`

ECS-to-scene sync exists in:

- `Scene/SceneManager.cpp:391` - `SyncECSToScene`
- `Scene/SceneManager.cpp:599` - creates missing `SceneObject`
- `Scene/SceneManager.cpp:690` - adds new object to active scene

### Diagnosis

The editor has two related data models:

- ECS entities/components
- Scene graph objects

The Scene Hierarchy reads from the active `Scene` and its `SceneObject` list. Many editor commands create ECS entities only. They do not immediately add matching `SceneObject` entries to the active scene.

`SceneManager::SyncECSToScene()` can repair this mismatch, but it is not called after every editor create/delete/duplicate operation. It is mainly used before saving and play-mode transitions.

### User-Visible Effect

Entities can exist in ECS but not appear in the Scene Hierarchy. The user sees an empty hierarchy even after creating objects.

### Recommended Fix

- After every editor entity create/delete/duplicate action, call `SceneManager::SyncECSToScene()`.
- If there is no active scene, create one before allowing entity creation.
- Better long-term fix: introduce an `EditorSceneService` that creates, deletes, duplicates, and renames entities through one API that updates ECS and Scene graph together.
- Make Scene Hierarchy capable of showing ECS-only orphan entities for diagnostics.

## 5. No Lighting And Color In The 3D World

### Code Evidence

Lighting upload is handled in:

- `RenderingEngine/Renderer/SceneRenderer.cpp:725`

Scene light detection occurs in:

- `RenderingEngine/Renderer/SceneRenderer.cpp:743`

Scene light collection depends on:

- `RenderingEngine/Renderer/SceneRenderer.cpp:755`
- `RenderingEngine/Renderer/SceneRenderer.cpp:757`

Light system/component registration appears in:

- `Core/World.h:24`
- `Core/World.h:65`
- `Core/World.h:66`
- `Core/World.h:67`
- `Core/World.h:68`
- `Core/World.h:137`
- `Core/World.h:139`
- `Core/World.h:142`

Light collection implementation is in:

- `ECS/LightCollectionSystem.h`

Editor-created light entities are implemented in:

- `Runtime/Private/Editor/Commands/EditorEntityCommands.cpp:202`
- `Runtime/Private/Editor/Commands/EditorEntityCommands.cpp:213`
- `Runtime/Private/Editor/Commands/EditorEntityCommands.cpp:224`
- `Runtime/Private/Editor/Commands/EditorEntityCommands.cpp:235`

### Diagnosis

The renderer has an editor default lighting fallback. Real scene lighting only applies when:

- `m_UseEditorDefaultLighting` is false.
- The ECS world has light components.
- `LightCollectionSystem` is registered and retrievable.
- Light entities are actually present in the active ECS world.

If scene/entity synchronization is broken, editor-created lights may not become part of the active scene workflow. If no default light is created for a new scene, the viewport can look flat, dark, or visually incorrect.

### Recommended Fix

- Add a default directional light and ambient light when creating a new scene.
- Ensure light creation syncs ECS to Scene graph immediately.
- Add a viewport lighting diagnostic overlay:
  - fallback lighting active/inactive
  - directional light count
  - point light count
  - ambient light found
  - `LightCollectionSystem` found/missing
- Make `Use Editor Default Lighting` state visible in the viewport toolbar.
- If scene has no lights, keep editor fallback lighting active by default.

## 6. Mouse And Keyboard Control Is Not Reliable

### Code Evidence

Editor camera control is implemented in:

- `Runtime/Public/Editor/EditorCamera.h:57`
- `Runtime/Public/Editor/EditorCamera.h:67`
- `Runtime/Public/Editor/EditorCamera.h:70`
- `Runtime/Public/Editor/EditorCamera.h:74`
- `Runtime/Public/Editor/EditorCamera.h:84`
- `Runtime/Public/Editor/EditorCamera.h:115`

Editor camera update is called from:

- `Runtime/Private/Editor/EditorLayer.cpp:348`
- `Runtime/Private/Editor/EditorLayer.cpp:349`

Viewport focus and hover state are set in:

- `Runtime/Private/Editor/Panels/ViewportPanel.cpp:133`
- `Runtime/Private/Editor/Panels/ViewportPanel.cpp:134`

Play-mode cursor capture is tracked by:

- `Runtime/Public/Editor/EditorLayer.h:83`
- `Runtime/Private/Editor/EditorLayer.cpp:456`
- `Runtime/Private/Editor/EditorLayer.cpp:458`
- `Runtime/Private/Editor/EditorLayer.cpp:468`
- `Runtime/Private/Editor/EditorLayer.cpp:2331`
- `Runtime/Private/Editor/EditorLayer.cpp:2359`

Player camera look uses:

- `ECS/PlayerControllerSystem.h:156`

### Diagnosis

Input is split between:

- ImGui direct polling
- `InputManager`
- player controller systems
- editor camera code

Editor camera movement depends on right mouse button drag and ImGui mouse delta. Play-mode camera movement depends on a boolean `m_CursorCaptured`, but the code path does not appear to strongly enforce OS/window cursor capture for all modes.

This causes:

- inconsistent viewport focus behavior
- keyboard input being swallowed by ImGui widgets
- mouse movement not feeling like a proper editor/game camera
- weak separation between edit-mode navigation and play-mode player input

### Recommended Fix

- Introduce a clear input ownership model:
  - Editor UI owns input when typing or interacting with widgets.
  - Viewport editor camera owns input during RMB drag.
  - Game/player owns input during play mode after cursor capture.
- Use platform/window mouse mode for play mode:
  - normal cursor in edit mode
  - confined/disabled cursor in play mode
  - restore cursor on Escape or Stop
- Avoid using ImGui mouse delta as the primary play-mode camera input.
- Route gameplay movement through `InputManager` or platform input consistently.
- Add visible viewport hints:
  - RMB + WASD for editor camera
  - click viewport to capture in play mode
  - Esc releases mouse

## Recommended Implementation Plan

### Phase 1: Make File Actions Usable

- Add native save dialog support.
- Use native open/save dialogs for File menu and top-bar buttons.
- Add editor notifications for action success/failure.
- Ensure Save As creates a scene if none exists.
- Validate in-memory scenes when no file path exists.

### Phase 2: Fix Scene Hierarchy Data

- Create an active scene automatically when editor starts or when first entity is created.
- Call `SceneManager::SyncECSToScene()` after create/delete/duplicate/rename/component changes.
- Add a debug section showing ECS entity count versus scene object count.

### Phase 3: Fill Edit Menu

- Add disabled placeholders for unavailable commands.
- Implement Duplicate/Delete/Rename using existing `EditorEntityCommands`.
- Add Undo/Redo only after a command history system exists.

### Phase 4: Fix Lighting Defaults

- Add default directional and ambient lights to new scenes.
- Keep editor fallback lighting active unless scene lighting is explicitly available.
- Add viewport lighting diagnostics.

### Phase 5: Improve Input Control

- Implement robust cursor capture/release through the window layer.
- Separate editor camera input and play-mode input.
- Stop relying on ImGui mouse delta for gameplay camera control.

## Highest Priority Root Causes

1. Editor menu actions fail silently or require manual path modals.
2. Scene Hierarchy reads the Scene graph, while many editor actions modify ECS only.
3. Edit menu is empty by implementation.
4. Lighting depends on scene/ECS sync and light collection state.
5. Input handling is split and lacks strong cursor ownership.

