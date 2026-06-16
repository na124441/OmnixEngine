# Rendering Fix Plan

## Critical Fixes

1. Fix material defaults.
   - Files: `Rendering/Materials/MaterialAsset.h`, `Runtime/Public/OmnixMaterialFormat.h`, material creation/migration code.
   - Task: change default metallic to `0.0f`, roughness to `0.6f`, base color to neutral grey/white, and clamp loaded materials with missing scalar data.
   - Risk: Low for new assets, Medium for existing assets because old scenes may visually change.
   - Validate with: `Assets/Scenes/render_diagnostic.omnixscene`, Albedo/Lit views.
   - Expected result: matte grey objects no longer behave like black/harsh metal.

2. Make shadow/depth layout state explicit.
   - Files: `Rendering/Core/Renderer.cpp`, `Rendering/Core/RenderTargetManager.*`, `Rendering/Graph/RenderGraph.*`.
   - Task: after `ShadowPass`, explicitly transition shadow depth image from depth attachment write to `VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL` with fragment shader read access, or teach render graph to do this from declared pass layouts. Update `RenderTargetManager.currentLayout` after render pass final layouts.
   - Risk: Medium because Vulkan layout changes affect many passes.
   - Validate with: Shadow Map debug mode and validation layers.
   - Expected result: no invalid layout warnings, stable shadow sampling.

3. Separate selection outline from deferred lighting.
   - Files: `shaders/deferred_lighting.glsl`, `Rendering/Editor/SelectionOutlinePass.*`, `Rendering/Core/Renderer.cpp`.
   - Task: remove orange outline from deferred lighting shader and implement a postprocess/editor overlay pass that reads object ID/depth and writes only to the editor viewport target.
   - Risk: Medium.
   - Validate with: selected object in Lit mode, screenshot with outline on/off.
   - Expected result: Lit HDR scene remains clean; selection is an editor overlay.

4. Audit vertex normal input contract.
   - Files: `Core/types/Vertex.h` or actual vertex header, `Rendering/Core/Renderer.cpp::initPipelines`, mesh importers/uploaders.
   - Task: ensure shader location 1 receives real normalized normals, not vertex color. Rename fields or create explicit `PbrVertex`.
   - Risk: High if many mesh paths depend on the old struct.
   - Validate with: Normal debug mode on cube/floor/sphere.
   - Expected result: coherent face normals and no harsh random lighting patches.

## High Priority Fixes

1. Add framebuffer extent validation.
   - Files: `Rendering/Core/FramebufferManager.cpp`, `Rendering/Core/RenderTargetManager.*`.
   - Task: before `vkCreateFramebuffer`, compare every attachment target extent with framebuffer desc width/height; log attachment debug names and sizes.
   - Risk: Low.
   - Validate with: resizing editor viewport repeatedly.
   - Expected result: resize bugs become explicit logs instead of broken viewport images.

2. Consolidate viewport color/depth ownership.
   - Files: `Rendering/Editor/EditorViewportRenderer.*`, `Rendering/Core/Renderer.cpp`.
   - Task: renderer owns render targets; editor viewport only registers the final image with ImGui.
   - Risk: Medium.
   - Validate with: offscreen viewport resize, Depth/Normal/Shadow debug modes.
   - Expected result: no stale offscreen depth/color resource ambiguity.

3. Clamp directional shadow distance before CSM.
   - Files: `Rendering/Core/Renderer.cpp`, directional light component/editor widgets.
   - Task: add `shadowDistance` default 75m and use it instead of full camera far plane for single-map directional shadow bounds.
   - Risk: Low/Medium.
   - Validate with: floor + cube shadow in diagnostic scene.
   - Expected result: sharper, less noisy near-field shadows.

4. Disable normal maps when tangent/UV data is invalid.
   - Files: importers, mesh metadata, `shaders/gbuffer_frag.glsl`.
   - Task: add mesh flags for hasUV/hasTangents; skip normal-map sampling when invalid.
   - Risk: Medium.
   - Validate with: Normal/Tangent debug modes.
   - Expected result: no garbage normal perturbation on incomplete meshes.

5. Make preview lighting explicit.
   - Files: `Rendering/Core/Renderer.h`, `Runtime/Private/Editor/Panels/ViewportPanel.cpp`, `RenderSceneExtractor.cpp`.
   - Task: rename and expose `m_UseEditorDefaultLighting`; default off when scene has lights; show fallback status in diagnostics.
   - Risk: Low.
   - Validate with: authored Sun/Sky entities.
   - Expected result: Lit mode matches scene lighting unless preview lighting is intentionally enabled.

## Medium Priority Improvements

1. Add real debug overlay pass.
   - Files: `Rendering/Debug/DebugDraw.*`, `Rendering/Editor/DebugViewRenderer.*`, `Renderer::setupRenderGraph`.
   - Task: render debug lines in a separate editor pass with depth test configurable and depth writes disabled.
   - Risk: Medium.
   - Validate with: collider/bounds/light overlays.
   - Expected result: debug visuals are opt-in and cannot alter scene lighting/material output.

2. Move grid to editor overlay.
   - Files: `Renderer::setupRenderGraph`, `Rendering/Editor/GridRenderer.*`.
   - Task: remove grid draw from `TransparentPass`; render it after postprocess or as a depth-aware editor overlay.
   - Risk: Low/Medium.
   - Validate with: Lit scene with grid off/on.
   - Expected result: runtime scene HDR target contains no editor grid.

3. Add texture mip generation.
   - Files: `RenderingEngine/Renderer/scene/Texture.cpp`.
   - Task: create mip levels, blit/generate mips during upload, set sampler LOD range.
   - Risk: Medium.
   - Validate with: textured floor at glancing camera angles.
   - Expected result: less shimmer/noise.

4. Add dedicated object ID target.
   - Files: G-buffer render pass/resource creation, `gbuffer_frag.glsl`, picking/selection code.
   - Task: replace packed ID in `GBufferC.b` with `R32_UINT` object ID attachment.
   - Risk: Medium/High.
   - Validate with: selecting entities with IDs > 255.
   - Expected result: no selection aliasing and cleaner material G-buffer.

## Long-Term Renderer Architecture

1. Make render graph own resource states.
   - Add per-pass declared layouts/access/stages for every input/output.
   - Generate barriers between passes.
   - Track render pass final layouts in one place.

2. Replace single directional shadow map with cascaded shadows.
   - Add cascade split computation, cascade matrices, array depth image, and cascade selection in deferred shader.
   - Keep current single-map path as a debug fallback until CSM is validated.

3. Add production viewport lighting.
   - Add sky irradiance, prefiltered environment reflections, BRDF LUT, and reflection capture fallback.
   - Calibrate direct light intensity/exposure around linear HDR.

4. Unify material data paths.
   - Remove dead per-material UBO from current deferred path or split forward/deferred materials clearly.
   - Add shader reflection tests that compare GLSL set/binding/type/stage with C++ layouts.

5. Establish renderer test scenes.
   - Keep `render_diagnostic.omnixscene` as a baseline.
   - Add automated captures or readbacks for albedo, normals, depth, shadow, and object ID.
