# Rendering Pipeline Map

This map reflects the active renderer compiled by `CMakeLists.txt`: `Rendering/Core/Renderer.cpp` plus the `Rendering/*` pass systems. The older `src/renderer/SceneRenderer.*` path is a separate sample/legacy renderer and is not the editor viewport pipeline.

## Frame Flow

1. `Renderer::BeginFrame()` in `Rendering/Core/Renderer.cpp`
   - Waits current frame fence.
   - Reads back previous GPU culling/occlusion counters.
   - Acquires the swapchain image with `vkAcquireNextImageKHR`.
   - Resets the per-frame command pool.
   - Selects viewport extent from `EditorViewportRenderer` offscreen size or swapchain extent.
   - Populates `FrameContext`.

2. `Renderer::RenderFrame(ECSWorld&, const CameraComponent&)`
   - Recreates depth/G-buffer/HDR/viewport resources if viewport extent changed.
   - Extracts ECS scene into `RenderScene` via `RenderSceneExtractor::ExtractScene`.
   - Builds `RenderQueue` / `transparentRenderQueue`.
   - Computes a single directional shadow light-space matrix.
   - Uploads camera, instance, material, light, object ID, local light, and cluster buffers through `GPUScene::UpdateFrame`.
   - Calls `setupRenderGraph()`, compiles, and executes graph passes.

3. `Renderer::EndFrame()`
   - Submits all physical pass command buffers for the frame.
   - Presents with `vkQueuePresentKHR`.
   - Advances `frameIndex`.

## Pass Table

| Pass | File / Function | Inputs | Outputs | Descriptor Sets | Pipeline | Color Writes | Depth Writes | Samples Depth/Shadow | Uses Lighting | Editor-only |
|---|---|---|---|---|---|---:|---:|---:|---:|---:|
| Frustum cull | `Rendering/Visibility/FrustumCullPass.cpp::Execute` called by `Renderer::setupRenderGraph` | GPU instance buffer, frustum UBO | visible instance buffer/count | pass-owned compute set | `m_FrustumCullPass` compute pipeline | No | No | No | No | No |
| Indirect command build | `Rendering/Visibility/IndirectCommandBuildPass.cpp::Execute` | visible instance buffer/count, mesh draw data | indirect draw buffer/count | pass-owned compute set | indirect command compute | No | No | No | No | No |
| Shadow pass | `Rendering/Core/Renderer.cpp::setupRenderGraph`, pass `"ShadowPass"` | render queue, `GPUScene` set 0, light-space matrix | `ShadowMap` | set 0 = `GPUScene` (`RadianceFrame`, instances, materials, lights, IDs, frustum, mesh data) | `m_ShadowPipeline` using `shaders/shadow_vert.spv`; depth-only | No | Yes | No | Directional light matrix only | Runtime-safe in concept |
| Depth prepass | `Renderer::setupRenderGraph`, pass `"DepthPrepass"` | render queue or indirect draw data | `DepthBuffer` | set 0 = `GPUScene`; optional cull/occlusion set | `m_DepthPipeline` or `m_DepthIndirectPipeline` | No | Yes | No | No | No |
| HZB pass | `Renderer::setupRenderGraph`, pass `"HZBPass"` | `DepthBuffer` | `HZBImage` | HZB compute descriptors | `m_HZBPass` compute | No | No | Yes, depth | No | No |
| Occlusion cull | `Renderer::setupRenderGraph`, pass `"OcclusionCullPass"` | `HZBImage`, frustum-visible buffer | final visible buffer/count | occlusion compute set | `m_OcclusionCullPass` compute | No | No | Yes, HZB | No | No |
| Final indirect rebuild | `Renderer::setupRenderGraph`, pass `"FinalIndirectCommandBuildPass"` | final visible buffer | final indirect draw commands | indirect compute set | indirect command compute | No | No | No | No | No |
| G-buffer pass | `Renderer::setupRenderGraph`, pass `"GBufferPass"` | depth, indirect command buffer, materials/textures | `GBufferA/B/C/D` | set 0 = `GPUScene`; set 1 = material textures; set 2 = visible instance buffer for indirect path | `m_GBufferIndirectPipeline` or material/G-buffer pipeline | Yes, 4 MRTs | Reads/tests depth; configured depth write state must be checked | No | No | No |
| SSAO | `Renderer::setupRenderGraph`, pass `"SSAOPass"` | depth, normal G-buffer, noise | `AO` | set 0 = `GPUScene`; set 1 = SSAO descriptors | `m_SSAOPipeline` | Yes | No | Yes, depth | No | No |
| SSAO blur | `Renderer::setupRenderGraph`, pass `"SSAOBlurPass"` | `AO` | `AOBlur` | SSAO blur set | `m_SSAOBlurPipeline` | Yes | No | No | No | No |
| Clustered light cull | `Renderer::setupRenderGraph`, pass `"LightCullingPass"` | local light buffer, cluster bounds/settings | cluster range/index buffers | `GPUScene` light-culling descriptor set | `m_LightCullingPipeline` | No | No | No | Yes, local light assignment | No |
| Deferred lighting | `Renderer::setupRenderGraph`, pass `"DeferredLightingPass"` | G-buffers, depth, shadow map, SSAO, light buffers | HDR color | set 0 = `GPUScene`; set 1 = G-buffer/depth/shadow/SSAO samplers; set 3 = local lights/clusters | `m_DeferredLightingPipeline` using `shaders/deferred_lighting.glsl` | Yes, HDR `R16G16B16A16_SFLOAT` | No | Yes, depth and shadow | Yes | No |
| Transparent + grid | `Renderer::setupRenderGraph`, pass `"TransparentPass"` | depth, HDR color | HDR color | set 0 = `GPUScene`; set 1 = material textures for transparent meshes | transparent material pipelines and `m_GridPipeline` | Yes | Transparent depth write disabled by material blend mode; grid state needs audit | Yes, depth attachment | Transparent shader uses light buffer; grid does not | Mixed: grid is editor-only but in lighting slot |
| Post process | `Renderer::setupRenderGraph`, pass `"PostProcessPass"` | HDR color | swapchain image or offscreen LDR image | set 0 = HDR sampler; set 1 = `GPUScene` camera/Radiance frame | `m_PostProcessPipeline` or `m_OffscreenPostProcessPipeline` | Yes | No | No | Exposure/tone map only | No |
| Editor overlay | `Renderer::setupRenderGraph`, pass `"EditorOverlayPass"` | LDR color | viewport color | none | none | Stub | No | No | No | Yes |
| UI / ImGui | `Renderer::setupRenderGraph`, pass `"UIPass"` | viewport/offscreen texture | swapchain/present | ImGui descriptors | ImGui backend pipeline | Yes when non-offscreen UI is drawn | No | No | No | Yes |

## Main Render Targets

| Target | Format | Created In | Usage |
|---|---|---|---|
| Depth | `VK_FORMAT_D32_SFLOAT` | `Renderer::recreateDepthResources` | depth attachment, sampled for HZB/SSAO/deferred |
| GBufferA | `VK_FORMAT_R8G8B8A8_UNORM` | `Renderer::recreateDepthResources` | albedo + flags |
| GBufferB | `VK_FORMAT_R16G16B16A16_SFLOAT` | `Renderer::recreateDepthResources` | world normal + roughness |
| GBufferC | `VK_FORMAT_R8G8B8A8_UNORM` | `Renderer::recreateDepthResources` | metallic, AO, packed entity ID |
| GBufferD | `VK_FORMAT_R8G8B8A8_UNORM` | `Renderer::recreateDepthResources` | emissive + shading model |
| ShadowMap | `VK_FORMAT_D32_SFLOAT` | `Renderer::createShadowResources` | depth attachment + sampled shadow texture |
| HDRColor | `VK_FORMAT_R16G16B16A16_SFLOAT` | `Renderer::recreateDepthResources` | deferred lighting output and transparent target |
| Offscreen viewport color | swapchain format | `EditorViewportRenderer::createOffscreenResources` | postprocess output displayed in ImGui |

## Descriptor Map

### `GPUScene` Set 0

Defined in `GPUScene::createDescriptorSetLayout` / `GPUScene::writeDescriptorSet`.

| Binding | Type | Shader Usage |
|---:|---|---|
| 0 | uniform buffer | `RadianceFrame` camera/sky/sun/exposure/render flags |
| 1 | storage buffer | instance transforms and material/object indices |
| 2 | storage buffer | material factors |
| 3 | storage buffer | `LightData` |
| 4 | storage buffer | object IDs |
| 5 | uniform buffer | frustum |
| 6 | storage buffer | mesh draw data |

### Material Set 1

Defined in `Material::allocateDescriptorSet`.

| Binding | Type | Shader Usage |
|---:|---|---|
| 0 | uniform buffer | Legacy per-material UBO, not used by current G-buffer shader |
| 1 | combined image sampler | albedo |
| 2 | combined image sampler | normal |
| 3 | combined image sampler | metallic-roughness |
| 4 | combined image sampler | AO |
| 5 | combined image sampler | emissive |

### Deferred G-buffer Set 1

Defined by `m_GBufferDescriptorSetLayout` and `Renderer::updateGBufferDescriptorSets`.

| Binding | Type | Image Layout Declared |
|---:|---|---|
| 0 | combined image sampler | GBufferA, shader-read |
| 1 | combined image sampler | GBufferB, shader-read |
| 2 | combined image sampler | GBufferC, shader-read |
| 3 | combined image sampler | depth, depth-stencil read-only |
| 4 | combined image sampler | GBufferD, shader-read |
| 5 | combined image sampler | shadow map, depth-stencil read-only |
| 6 | combined image sampler | SSAO blur, shader-read |

## Runtime Safety Notes

- The grid is currently rendered inside `TransparentPass`, not a separate editor overlay pass.
- Selection outline is implemented in `shaders/deferred_lighting.glsl` by reading packed entity ID from `GBufferC` and writing orange directly into HDR lighting output.
- `EditorOverlayPass` is registered but is a stub, so most editor visuals are either in the deferred shader or ImGui draw lists, not a clean renderer overlay layer.
- The render graph sorts passes and validates handles, but it does not own image layout transitions for render targets. Many transitions are manual or implicit through render pass final layouts.
