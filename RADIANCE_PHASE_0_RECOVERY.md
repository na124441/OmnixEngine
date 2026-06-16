# Radiance Phase 0 Recovery Report

## Overview
This recovery phase focuses on stabilizing the broken renderer, preventing crashes on startup due to unsafe experimental indirect visibility modes, and isolating the editor overlays from the real mesh rendering.

## Actions Taken & Stabilizations
1. **Disabled Unsafe Indirect Modes by Default**:
   - Changed the default visibility mode from `GPUFrustumIndirect` to `CPUDriven` in [Renderer.h](file:///d:/OmnixEngine/Rendering/Core/Renderer.h).
   - This ensures the engine starts safely without crashes.
2. **Added Editor Visual Warnings & Logging**:
   - Marked experimental visibility modes (`GPUFrustumIndirect` and `GPUFrustumOcclusion`) with `Experimental:` prefixes in the Editor UI.
   - Added runtime log warnings (`CORE_LOG_WARN`) when experimental indirect modes are selected or active during frame rendering in [Renderer.cpp](file:///d:/OmnixEngine/Rendering/Core/Renderer.cpp).
3. **Editor Overlay Isolation**:
   - Added `showGizmos`, `showLightVolumes`, and `showLabels` toggles to `ViewportOverlaySettings` in [RenderTypes.h](file:///d:/OmnixEngine/Rendering/Core/RenderTypes.h).
   - Exposed getters and setters for these overlays in [ViewportPanel.h](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/ViewportPanel.h).
   - Wrapped viewport Axis/Transform Gizmos, 2D Labels, and Light debug guides under toggle checks in [ViewportPanel.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/Panels/ViewportPanel.cpp).
   - Added toolbar checkboxes and exposed toggles under the `View` menu (including a "Clean Viewport Mode" quick-toggle) in [EditorLayer.cpp](file:///d:/OmnixEngine/Runtime/Private/Editor/EditorLayer.cpp).
   - Under `LitNoOverlays` / Clean viewport modes, disabled all overlays.
4. **Test Scene Creation**:
   - Created a reliable test scene: [RendererTest_Radiance_v05.omnixscene](file:///d:/OmnixEngine/Assets/Scenes/RendererTest_Radiance_v05.omnixscene).
