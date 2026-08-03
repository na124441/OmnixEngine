# Rendering Diagnosis

The viewport is not failing because of a single UI color or one stretched cube. The active renderer is a partial deferred Vulkan renderer with shadow, G-buffer, SSAO, clustered-light, postprocess, and editor systems, but several engine-level contracts are incomplete or internally inconsistent. The result is a debug-like viewport with unstable lighting, suspect shadows, material defaults that bias objects toward metal, and editor overlays mixed into the lit scene.

## Lighting Pipeline Findings

### Critical: Scene lighting can be bypassed by editor fallback lighting

- File/function: `Renderer.h`, `RenderSceneExtractor::ExtractScene`, `Renderer::RenderFrame`.
- Evidence: `Renderer::m_UseEditorDefaultLighting` defaults to `true`. `ExtractScene` uses hardcoded sun and sky whenever this is true, even if scene lights exist.
- Why it affects quality: the editor can show a scene that does not match authored Sun/Sky/Point/Spot light components. That makes lighting look flat or "wrong" even when scene light entities are configured.
- Suggested fix: expose a clear viewport toggle named `Use Preview Lighting`, default it off for Lit mode when a scene has lights, and show a viewport badge when fallback lighting is active.

### High: `LightCollectionSystem` fallback reports lights as enabled without copying scene shadow data

- File/function: `ECS/LightCollectionSystem.h::CollectLights`, `RenderSceneExtractor::ExtractLighting`.
- Evidence: if no directional light is found, `data.directionalLight.enabled = true` is forced with default direction/intensity. The runtime light structs do not include shadow bias/strength/resolution fields.
- Why it affects quality: code paths that use `LightCollectionSystem` can silently light the scene even when authored light collection is incomplete, and they cannot preserve directional shadow authoring.
- Suggested fix: make absence of authored directional light explicit, carry shadow parameters in `RuntimeDirectionalLight`, and avoid converting "not found" into "enabled" without a diagnostic flag.

### High: Deferred shader uses `frame.sunDirectionIntensity`, while light buffer also contains directional light data

- File/function: `shaders/deferred_lighting.glsl::EvaluateDirectionalLight`, `GPUScene::UpdateFrame`, `Renderer::UpdateRadianceFrameUBO`.
- Evidence: the deferred shader declares `LightBuffer` but directional BRDF reads sun direction/color/intensity from `RadianceFrame` (`frame.sunDirectionIntensity`, `frame.sunColorAngularSize`), while shadow settings and ambient use `LightBuffer`.
- Why it affects quality: CPU code must keep two independent GPU structs synchronized. If `RadianceFrame` and `LightData` diverge, direct light and shadow projection use different light directions.
- Suggested fix: pick one source of truth. Prefer `LightData` for scene lighting and reserve `RadianceFrame` for camera/sky/exposure. Add a debug assert/log comparing the two until refactored.

### High: Normal-map handling is unsafe without mesh tangents

- File/function: `shaders/gbuffer_frag.glsl::perturbNormal`; `shaders/pbr_frag.glsl` legacy forward path.
- Evidence: deferred path reconstructs TBN from derivatives; forward path adds tangent-space normal samples directly to world normal (`N = normalize(vNormal + normalSample)`).
- Why it affects quality: derivative TBN can be acceptable as a fallback, but it is unstable with bad UVs, mirrored UVs, discontinuities, and missing UVs. The forward shader is technically wrong for normal maps.
- Suggested fix: add tangents to imported/uploaded vertex data, store tangent handedness, and disable normal mapping when tangents/UVs are absent. Remove or quarantine the legacy forward normal path.

### Medium: Ambient and sky terms are simple constant multipliers, not IBL

- File/function: `shaders/deferred_lighting.glsl`, `shaders/pbr_frag.glsl`.
- Evidence: ambient is `ambientColorIntensity.rgb * intensity * albedo * AO`; no irradiance map, prefiltered reflection, BRDF LUT, or reflection probes.
- Why it affects quality: even when the math is valid, materials cannot look like modern PBR in an Unreal/Unity-style viewport without environment lighting and specular IBL.
- Suggested fix: add irradiance + prefiltered environment maps or a simpler editor preview cubemap before tuning material appearance.

## Shadow Pipeline Findings

### Critical: Shadow map layout/state tracking is not authoritative

- File/function: `Renderer::createShadowResources`, `Renderer::updateGBufferDescriptorSets`, `RenderTargetManager::Transition`.
- Evidence: shadow render pass final layout is `VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL`, and descriptor update declares the same. However `RenderTargetManager` initializes `currentLayout` to `UNDEFINED` and is not updated by render pass automatic transitions. The render graph validates handles but does not reconcile actual layouts.
- Why it affects quality: this creates a gap between Vulkan's implicit render pass layout transitions, descriptor declarations, and engine-side layout tracking. Any later explicit transition can use stale oldLayout and produce validation errors or undefined sampling.
- Suggested fix: centralize render-target state. After a render pass with a known final layout, update the tracked layout, or make render graph own barriers and render pass layout contracts.

### High: Shadow pass dependency does not explicitly synchronize depth writes to fragment shader sampling

- File/function: `Renderer::createShadowResources`.
- Evidence: final layout is read-only, but dependencies are depth-stage oriented. The main deferred shader samples `shadowMap` as a regular sampler2D.
- Why it affects quality: weak synchronization around a depth attachment sampled later can produce flicker or stale data on some drivers.
- Suggested fix: add an explicit barrier after `ShadowPass` from depth attachment write to fragment shader read, or express it in a render-graph transition system.

### High: Directional shadow projection covers the full camera far plane

- File/function: `Renderer::RenderFrame`, shadow light-space matrix calculation.
- Evidence: shadow frustum radius is computed using `activeRenderScene.camera.farPlane`, defaulting to 1000.0.
- Why it affects quality: a 2048 shadow map spread over a huge camera frustum gives extremely low texel density, causing muddy/jagged/noisy shadows and acne sensitivity.
- Suggested fix: clamp editor shadow distance to a practical range such as 50-100m, then add cascaded shadow maps for production.

### Medium: Only one non-cascaded directional shadow exists

- File/function: `RenderScene.h::DirectionalLightGPU`, `Renderer::RenderFrame`, `shaders/deferred_lighting.glsl`.
- Evidence: `CascadedShadowData` exists in `Renderer.h`, but the active shader samples one `shadowMap` and one `directionalLightProjView`.
- Why it affects quality: single-map directional shadows cannot be clean across both near editor objects and far scene content.
- Suggested fix: implement CSM after the current single-map path is correct and validated.

### Medium: Shadow sampler is non-comparison and PCF is manual

- File/function: `Renderer::createShadowResources`, `shaders/deferred_lighting.glsl::CalculateShadow`.
- Evidence: shader samples `sampler2D shadowMap` and compares manually; sampler compare mode is not used.
- Why it affects quality: manual PCF can work, but it increases the chance of reversed-depth/bias mistakes and makes filtering quality dependent on ad hoc shader code.
- Suggested fix: keep manual PCF for now but add `ShadowMapPreview`, shadow coordinate debug, bias sliders, and assertions for valid shadow map layout.

## Material/PBR Pipeline Findings

### Critical: Default material is metallic by default

- File/function: `Rendering/Materials/MaterialAsset.h`, `Runtime/Public/OmnixMaterialFormat.h`.
- Evidence: `MaterialAsset::metallicFactor = 1.0f`, `MaterialGPU::metallicFactor = 1.0f`, and `OmnixMaterial::metallicFactor = 1.0f`.
- Why it affects quality: missing or newly created materials become fully metallic. A grey cube or floor will behave like metal, creating harsh/black reflections in a renderer that lacks proper IBL.
- Suggested fix: change defaults to metallic `0.0f`, roughness `0.6f`, base color neutral grey/white. Add migration/clamping for old material files.

### High: G-buffer stores entity ID in an 8-bit normalized color channel

- File/function: `shaders/gbuffer_frag.glsl`, `shaders/deferred_lighting.glsl`.
- Evidence: `outGBufferC.b = float(vEntityID) / 255.0`; deferred pass reconstructs with `round(gbufferCSample.b * 255.0)`.
- Why it affects quality: entity IDs above 255 alias, and `GBufferC` is also used for material data. Selection outline is therefore tied to lossy material render output.
- Suggested fix: move picking/selection ID to a dedicated `R32_UINT` attachment or separate picking pass. Keep material G-buffer channels physically meaningful.

### High: Albedo color space is inconsistent across fallback and G-buffer storage

- File/function: `Texture::loadFromFile`, `Texture::getWhiteTexture`, `shaders/gbuffer_frag.glsl`.
- Evidence: albedo/emissive file textures use `VK_FORMAT_R8G8B8A8_SRGB`; fallback white uses `UNORM`. GBufferA is `UNORM` and stores sampled albedo directly.
- Why it affects quality: file albedo samples are converted to linear by Vulkan, but fallback/constants may not follow the same assumptions. GBufferA `UNORM` also compresses linear values before lighting.
- Suggested fix: document linear/sRGB convention, use linear HDR-friendly G-buffer formats where needed, and make fallback albedo behavior match shader expectations.

### Medium: No mip generation for textures

- File/function: `Texture::loadFromFile`.
- Evidence: textures are created with `mipLevels = 1`; sampler uses mipmap mode but there are no mip levels.
- Why it affects quality: textured surfaces shimmer and look noisy in the viewport.
- Suggested fix: generate mipmaps on upload or force sampler mip mode to nearest with LOD 0 until mip generation exists.

### Medium: Current shader is partial PBR, not production PBR

- Evidence: deferred shader uses GGX/Smith/Fresnel for direct lights, but lacks IBL, clearcoat, transmission, energy-calibrated lights, reflection probes, and robust material validation.
- Classification: partial/debug PBR-style direct lighting, not a production-style lit material pipeline.

## Mesh Data Findings

### High: Vertex layout appears to use color as normal in active pipeline creation

- File/function: `Renderer::initPipelines`.
- Evidence: pipeline vertex attributes use `offsetof(Vertex, pos)`, `offsetof(Vertex, color)` at location 1, and `offsetof(Vertex, uv)` at location 2, while shaders name location 1 `inNormal`.
- Why it affects quality: if `Vertex::color` is not actually normal data, lighting normals are wrong, causing yellow/black faceting and unstable shading.
- Suggested fix: audit `Core/types/Vertex.h` and all mesh import/upload paths. Rename or split vertex structs so shader location 1 always receives normalized normals.

### High: Normal and tangent validation is not enforced at import

- File/function: `Runtime/Private/MeshImporter.cpp`, `OBJImporter`, `GLTFImporter`, mesh upload.
- Evidence: current shader supports normal mapping without guaranteed tangents; diagnostic report found no hard block for missing tangents in the active shader path.
- Why it affects quality: missing/invalid normals and tangents produce broken lighting even if descriptor and render pass logic is correct.
- Suggested fix: validate normals, generate normals when safe, generate MikkTSpace tangents for glTF/OBJ, and disable normal maps when UV/tangent data is invalid.

## Descriptor/Shader Binding Findings

### Confirmed: Deferred descriptor sets mostly match GLSL declarations

- File/function: `shaders/deferred_lighting.glsl`, `GPUScene::createDescriptorSetLayout`, `Renderer::updateGBufferDescriptorSets`, `Renderer::initPipelines`.
- Evidence: set 0 binding 0/3 and set 1 binding 0-6 are created and bound; set 3 bindings 0-4 match local light/cluster buffers.
- Why it matters: the core deferred pass is not obviously missing an entire light buffer binding.
- Suggested fix: keep this mapping documented and add reflection/assert tests so future shader changes cannot silently desync.

### High: Material set layout has an unused binding 0 UBO while current material data comes from set 0 binding 2

- File/function: `Material::allocateDescriptorSet`, `shaders/gbuffer_frag.glsl`.
- Evidence: material descriptor set writes binding 0 UBO, but G-buffer shader reads material factors from `MaterialBuffer` set 0 binding 2.
- Why it affects quality: there are two material data paths. Editing code updates `Material::uboData`, then `GPUScene::UpdateFrame` copies it to the SSBO, but the descriptor UBO is dead for the current deferred shader.
- Suggested fix: remove or mark legacy material UBO, or keep it only for forward/transparent pipelines with explicit documentation.

### Medium: Legacy forward PBR shader expects a smaller/different `MaterialData`

- File/function: `shaders/pbr_frag.glsl` vs `shaders/gbuffer_frag.glsl`.
- Evidence: forward `MaterialData` has `albedoColor`, `roughness`, `metallic`, `hasAlbedoMap`, `hasNormalMap`; G-buffer uses `baseColorFactor`, `roughnessFactor`, `metallicFactor`, `normalScale`, etc.
- Why it affects quality: if a pipeline accidentally uses `pbr_frag.glsl` with current `MaterialGPU`, fields will be read incorrectly.
- Suggested fix: delete, rename, or isolate legacy shader variants; add shader-layout reflection tests.

## Framebuffer/Viewport Extent Findings

### High: Offscreen viewport has its own depth resources and renderer also creates offscreen depth framebuffers

- File/function: `EditorViewportRenderer::createOffscreenResources`, `Renderer::recreateDepthResources`.
- Evidence: `EditorViewportRenderer` creates `m_OffscreenDepthImages`, while `Renderer` also has `m_OffscreenDepthFramebuffers` and depth handles.
- Why it affects quality: duplicate depth ownership increases the risk of sampling or rendering against stale/wrong depth images after viewport resize.
- Suggested fix: make a single owner for viewport color/depth attachments. The renderer should own render targets; the editor viewport should only own ImGui texture registration.

### Medium: Framebuffer manager does not validate attachment extents against framebuffer dimensions

- File/function: `FramebufferManager::RebuildInvalidated`.
- Evidence: it gathers image views and creates framebuffer using `desc.width/height`, but does not compare each attachment target extent with those dimensions.
- Why it affects quality: extent mismatch can cause validation errors or broken viewport rendering after resize.
- Suggested fix: add explicit extent checks and log target names/sizes before `vkCreateFramebuffer`.

## Render State and Pass Ordering Findings

### Critical: Render graph uses multiple logical passes in the same physical command buffer slot without ownership of image layouts

- File/function: `RenderGraph::ExecuteWithValidation`, `Renderer::setupRenderGraph`.
- Evidence: several passes use `PassID::Geometry` and several use `PassID::Lighting`; render graph records them sequentially but does not insert generic image barriers from declared inputs/outputs.
- Why it affects quality: pass order is sorted, but resource state is still manually patched in individual passes. Missing one transition can corrupt shadow/depth/G-buffer sampling.
- Suggested fix: make render graph resource declarations include required layouts and insert transitions automatically, or add a strict manual transition table for every pass.

### High: Selection outline is inside deferred lighting

- File/function: `shaders/deferred_lighting.glsl`, selected object outline block.
- Evidence: selected ID check runs before background and shading, and writes orange directly to `outColor`.
- Why it affects quality: editor selection contaminates the actual scene color/HDR buffer. It is not a separate overlay that can be disabled independently from lighting.
- Suggested fix: move selection to a separate overlay pass after postprocess or to a stencil/ID-driven outline pass writing only the editor viewport.

### Medium: Grid renders in transparent pass

- File/function: `Renderer::setupRenderGraph`, `"TransparentPass"`.
- Evidence: grid is drawn before transparent meshes inside the HDR lighting slot.
- Why it affects quality: an editor-only visual is mixed into the scene HDR buffer and tone mapped with scene lighting.
- Suggested fix: render grid in an editor overlay pass after main scene, with independent opacity/depth behavior.

## Editor Overlay Contamination Findings

### High: ImGui viewport panel draws selection, collider, bounds, and light gizmo wireframes over the rendered texture

- File/function: `Runtime/Private/Editor/Panels/ViewportPanel.cpp`.
- Evidence: the panel projects world points and draws wireframe boxes/spheres/cones via ImGui draw lists for selection, colliders, point lights, and spot lights.
- Why it affects quality: even if the scene renderer is clean, the final viewport can resemble a debug renderer when collider/light overlays are enabled by default.
- Suggested fix: default heavy overlays off in Lit mode; add separate toolbar toggles and persist them per viewport mode.

### Medium: `DebugDraw` accumulates debug lines but no clean renderer pass consumes them in the main graph

- File/function: `Rendering/Debug/DebugDraw.cpp`, `RenderSceneExtractor::ExtractLocalLights`, `Renderer::populateVisibilityDebugDraw`.
- Evidence: debug lines are pushed to a static vector, but the active render graph has only a stub `EditorOverlayPass`.
- Why it affects quality: debug infrastructure is partially integrated, increasing confusion over which overlays are GPU-rendered versus ImGui-rendered.
- Suggested fix: implement a real debug line overlay pass with depth-test on/off options and explicit clear timing.

## GPU Capture / Runtime Evidence

RenderDoc integration is not currently bound. `Renderer::RequestRenderDocCapture` only logs that capture API integration is not bound yet.

Use the existing debug views from the viewport mode menu:

- `Albedo`: `m_ShadingMode = 10`
- `Normal`: `m_ShadingMode = 3`
- `Depth`: `m_ShadingMode = 2`
- `Shadow Map`: `m_ShadingMode = 9`
- `Wireframe-style edges`: `m_ShadingMode = 11`
- `Light Complexity`: `m_ShadingMode = 12`
- `Tangent`: `m_ShadingMode = 13`

Recommended capture/log sequence:

1. Load `Assets/Scenes/render_diagnostic.omnixscene`.
2. Enable Lit, no editor overlays.
3. Check Shadow Map mode: it should show non-uniform depth, not all white/black.
4. Check Normal mode: cube/floor faces should be coherent flat colors per face.
5. Check Albedo mode: material colors should be neutral, not yellow/black.
6. Check Depth mode: floor/object depth should form a stable gradient.
7. If RenderDoc is installed later, capture ShadowPass and DeferredLightingPass and inspect set 1 binding 5 image layout and contents.

## Final Diagnosis Classification

| Category | Status | Evidence | Next Fix |
|---|---|---|---|
| Broken shader/descriptor binding | Likely | Layouts mostly match deferred path, but legacy forward shader material layout differs and material set has dead UBO path. | Add shader reflection/layout tests and remove legacy ambiguity. |
| Broken material/PBR model | Confirmed | Default metallic is 1.0 and renderer lacks IBL. | Change defaults, migrate materials, add material validation/debug scene. |
| Broken mesh normals/tangents/import | Likely | Active pipeline maps shader normal input to `Vertex::color`; no tangent contract. | Audit vertex struct/importers, generate normals/tangents, add normal debug validation. |
| Broken shadow map generation/sampling | Likely | Single full-frustum shadow map, weak explicit layout ownership, no CSM. | Add explicit shadow barrier/state tracking, clamp shadow distance, validate shadow preview. |
| Broken framebuffer/image layout transitions | Confirmed | Render target manager layout state is not updated by render pass final layouts; render graph does not own transitions. | Centralize layout state and add assertions before descriptor updates. |
| Broken render pass order | Possible | Logical order is reasonable, but manual transitions are incomplete-risk. | Add render-graph layout transition table and validation logs. |
| Debug overlay contamination | Confirmed | Selection is in deferred shader; grid is in transparent HDR pass; viewport ImGui draws overlays. | Move editor visuals to separate overlay pass and default heavy overlays off. |
| Weak but technically valid renderer architecture | Confirmed | Deferred pipeline exists but has split light sources, duplicate viewport depth ownership, and partial render graph. | Consolidate ownership and contracts before adding features. |
| Missing production viewport features | Confirmed | No IBL, no CSM, no TAA/AA, no material preview environment, no robust debug pass separation. | Add features after correctness fixes. |
| Editor UX/default scene configuration issue | Possible | Current test scenes use yellow sun/color and debug collider flags. | Use neutral diagnostic scene and Lit defaults with overlays off. |

## Minimal Diagnostic Scene

`Assets/Scenes/render_diagnostic.omnixscene` has been added as a neutral scene definition. It is intentionally simple and should be used to verify:

- Lit mode shows neutral grey materials.
- Shadow Map mode shows valid depth.
- Albedo mode shows neutral albedo.
- Normal mode shows coherent normals.
- Depth mode shows a stable gradient.
- No heavy yellow wireframe appears unless editor overlays are explicitly enabled.
