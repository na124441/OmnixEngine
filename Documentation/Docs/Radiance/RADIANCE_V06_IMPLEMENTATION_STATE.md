# Omnix Radiance v0.6 Implementation State

## Target Architecture Version
0.6

## Current Milestone
I0

## Current Milestone Status
COMPLETED

## Last Completed Milestone
I0 (Renderer Contract Repair)

## Completed Exit Gates
* [x] Depth/reconstruction trustworthy — Measured distances and camera motion pass.
* [x] Object identity works — Picking/outline use R32_UINT.
* [x] Resize safe — No stale descriptors or validation errors.
* [x] Dependencies explicit — Synchronization validation clean.
* [x] Resources documented — Runtime inventory matches documentation.

## Failed Exit Gates
None

## Architecture Contracts Implemented
* Vulkan Standard Zero-to-One Depth coordinate conventions.
* CPU-side camera/UBO matrix pre-inversions (optimizing out fragment shader `inverse()` calls).
* Dedicated G-buffer `VK_FORMAT_R32_UINT` ObjectID render target and descriptor set bindings.
* Clean RenderGraph layout transitions and overlay pass scheduling.

## Existing Systems Extended
* `Renderer` geometry/swapchain framebuffer configurations.
* Camera frustum culling setup (`GPUScene`) and scene camera extractor.
* Entity picking staging buffer readback logic.

## New Systems Introduced
* Viewport color rendering pass (`m_ViewportColorRenderPass`) and overlay scheduling.

## Current Frame Resource Layout
* `ShadowMap`: `D32_SFLOAT` (directional light depth map)
* `DepthBuffer`: `D32_SFLOAT` (main scene depth buffer)
* `GBufferA`: `R8G8B8A8_UNORM` (base color RGB)
* `GBufferB`: `R16G16B16A16_SFLOAT` (world space normal XYZ, roughness W)
* `GBufferC`: `R8G8B8A8_UNORM` (metallic R, AO G, padding B/A)
* `GBufferD`: `R8G8B8A8_UNORM` (emissive RGB, shading model A)
* `GBufferObjectID`: `R32_UINT` (unsigned 32-bit pixel-perfect Entity ID)
* `SSAO`: `R8_UNORM` (raw screenspace ambient occlusion)
* `AOBlur`: `R8_UNORM` (blurred screenspace ambient occlusion)
* `HDRColor`: `R16G16B16A16_SFLOAT` (composited scene color before tonemapping)
* `LDRColor`: `R8G8B8A8_UNORM` (exposure-corrected and tonemapped viewport color)
* `ViewportColor`: `R8G8B8A8_UNORM` (composited final color with grid and outlines)
* `Swapchain`: Swapchain color buffer
* `Backbuffer`: Backbuffer target abstraction

## Current Descriptor Layout
* **Descriptor Set 0 (Global Frame)**:
  * Binding 0: `GlobalUBO` (Camera/Sky/Sun/Exposure/Render Flags)
* **Descriptor Set 1 (Material/GBuffer)**:
  * Binding 0: `MaterialUBO`
  * Bindings 1-5: Texture samplers (albedo, normal, metallicRoughness, AO, emissive)
  * Binding 6: `sampler2D ssaoBlur`
  * Binding 7: `usampler2D objectIDTex` (newly bound to dedicated ObjectID view)
* **Descriptor Set 2 (Lighting & Shadows)**:
  * Binding 0: `LightingUBO`
  * Binding 1: `sampler2D shadowMap`

## Current RenderGraph Order
1. `ShadowPass`
2. `DepthPrepass`
3. `GBufferPass`
4. `SSAOPass`
5. `SSAOBlurPass`
6. `LightCullingPass`
7. `DeferredLightingPass`
8. `TransparentPass`
9. `EditorOverlayPass` (renders grid and selection outline on `ViewportColor`)
10. `PostProcessPass`
11. `UIPass`
12. `PresentPass`

## Current Shader Contracts
* `gbuffer_vert.glsl` / `gbuffer_frag.glsl` outputting color, normal, roughness, metallic, AO, emissive, and unsigned ObjectID.
* `deferred_lighting.glsl` sampling G-buffer, reading pre-inverted matrices, and resolving direct PBR.

## Current Depth Convention
Standard zero-to-one (clear to `1.0f`, compare operator `VK_COMPARE_OP_LESS`).

## Current Color-Space Convention
sRGB for file textures, linear for GPU computations, linear G-Buffer format storage, sRGB display encoding during post-processing.

## Current Temporal Histories
None

## Known Defects
None (all Milestone I0 defects resolved).

## Known Limitations
* SSAO is still functional but lacks temporal filters (to be addressed in GI milestones).

## Deferred Work
None

## Performance Baseline
Build times are stable; GPU optimizations eliminated 2 heavy per-pixel shader matrix inversions.

## Vulkan Validation Status
Clean (0 validation errors during viewport resizing and entity selection).

## Synchronization Validation Status
Clean (no hazards or desynchronization).

## Recommended Next Action
Obtain permission to start Milestone I1.

## Last Updated Workspace or Commit
fix/gbuffer-geometry-pipeline
