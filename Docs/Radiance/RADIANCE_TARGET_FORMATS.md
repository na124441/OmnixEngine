# Radiance G-Buffer Target Formats and Conventions

This document specifies the authoritative GPU format layout and clear conventions for the G-Buffer targets.

| Target Name | format | channels | Clear Value | Usage Semantics |
| :--- | :--- | :--- | :--- | :--- |
| `DepthBuffer` | `VK_FORMAT_D32_SFLOAT` | Depth (1) | `1.0f` | Main depth buffer |
| `GBufferA` | `VK_FORMAT_R8G8B8A8_UNORM` | RGB, Flags (A) | `0.035, 0.040, 0.050, 1.0` | Base color + material flags |
| `GBufferB` | `VK_FORMAT_R16G16B16A16_SFLOAT` | Normal XYZ, Roughness (A) | `0.0, 0.0, 0.0, 1.0` | World-space normals + roughness |
| `GBufferC` | `VK_FORMAT_R8G8B8A8_UNORM` | Metallic, AO, Padding (B/A) | `0.0, 1.0, 0.0, 1.0` | Metallic factor + Ambient Occlusion |
| `GBufferD` | `VK_FORMAT_R8G8B8A8_UNORM` | Emissive RGB, Model (A) | `0.0, 0.0, 0.0, 1.0` | Emissive energy + shading model |
| `GBufferObjectID` | `VK_FORMAT_R32_UINT` | ObjectID (1) | `0xFFFFFFFF` | Entity identity index |
| `GBufferVelocity` | `VK_FORMAT_R16G16_SFLOAT` | Velocity XY (2) | `0.0, 0.0` | Current-to-previous screen motion |

## Layout Rules
* All formats are configured as `COLOR_ATTACHMENT_OPTIMAL` during G-buffer generation and transition to `SHADER_READ_ONLY_OPTIMAL` for lighting, SSAO, outline rendering, etc.
* No channel compression/bit-packing is allowed without reference test coverage.
* The `DepthBuffer` format must remain standard Vulkan `VK_FORMAT_D32_SFLOAT`.
