# Radiance Frame Resources Inventory

This document lists all active GPU resources used in the Omnix Radiance rendering pipeline, detailing their format, clear behavior, lifecycle, layout states, and occupancy.

## 1. Viewport-Sized Textures

| Resource Name | Format | Clear Value | Producer Pass | Consumer Pass(es) | Layout Transition Flow | Lifetime / Duplication |
|---|---|---|---|---|---|---|
| `DepthBuffer` | `VK_FORMAT_D32_SFLOAT` | `1.0f` | `DepthPrepass` / `GBufferPass` | `SSAOPass`, `DeferredLightingPass`, `TransparentPass` | `UNDEFINED` → `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` → `DEPTH_STENCIL_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `GBufferA` | `VK_FORMAT_R8G8B8A8_UNORM` | `0.035, 0.040, 0.050, 1.0` | `GBufferPass` | `DeferredLightingPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `GBufferB` | `VK_FORMAT_R16G16B16A16_SFLOAT` | `0.0, 0.0, 0.0, 1.0` | `GBufferPass` | `DeferredLightingPass`, `SSAOPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `GBufferC` | `VK_FORMAT_R8G8B8A8_UNORM` | `0.0, 1.0, 0.0, 1.0` | `GBufferPass` | `DeferredLightingPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `GBufferD` | `VK_FORMAT_R8G8B8A8_UNORM` | `0.0, 0.0, 0.0, 1.0` | `GBufferPass` | `DeferredLightingPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `GBufferObjectID` | `VK_FORMAT_R32_UINT` | `0xFFFFFFFF` | `GBufferPass` | `SelectionOutlinePass`, `PickEntity` readback | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `SSAO` | `VK_FORMAT_R8_UNORM` | `1.0` | `SSAOPass` | `SSAOBlurPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized / Half-res. 1 per frame-in-flight. |
| `AOBlur` | `VK_FORMAT_R8_UNORM` | `1.0` | `SSAOBlurPass` | `DeferredLightingPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `HDRColor` | `VK_FORMAT_R16G16B16A16_SFLOAT` | `0.0, 0.0, 0.0, 1.0` | `DeferredLightingPass` | `PostProcessPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `LDRColor` | `VK_FORMAT_R8G8B8A8_UNORM` | `0.0, 0.0, 0.0, 1.0` | `PostProcessPass` | `EditorOverlayPass` / `UIPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |
| `ViewportColor` | `VK_FORMAT_R8G8B8A8_UNORM` | `0.0, 0.0, 0.0, 1.0` | `EditorOverlayPass` | `UIPass` | `UNDEFINED` → `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` | Viewport-sized. 1 per frame-in-flight. |

## 2. Shadow Map Resources

| Resource Name | Format | Clear Value | Producer Pass | Consumer Pass(es) | Layout Transition Flow | Lifetime / Duplication |
|---|---|---|---|---|---|---|
| `ShadowMap` | `VK_FORMAT_D32_SFLOAT` | `1.0f` | `ShadowPass` | `DeferredLightingPass` | `UNDEFINED` → `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` → `DEPTH_STENCIL_READ_ONLY_OPTIMAL` | `2048x2048`. 1 per frame-in-flight. |

## 3. Persistent Buffer Allocations

| Allocation Name | GPU Buffer Type | Size (Bytes) | Alignment | Update Frequency | Lifetime | Description / Binding |
|---|---|---|---|---|---|---|
| `CameraBuffer` | `VkBuffer` (Uniform) | `sizeof(RadianceFrameUBO)` | 16 | Once per frame | Persistent | Descriptor Set 0 Binding 0 (View, projection, and inverse matrices) |
| `LightBuffer` | `VkBuffer` (Storage) | `sizeof(LightData)` | 16 | Once per frame | Persistent | Descriptor Set 2 Binding 0 (Compact light records, colors, and shadow projection matrices) |
