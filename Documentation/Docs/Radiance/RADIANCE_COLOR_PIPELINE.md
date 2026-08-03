# Radiance Color Pipeline and Space Conversions

This document defines color space conventions and gamma transfer procedures inside the Omnix Engine rendering pipeline.

## 1. Storage Space vs Shading Space
* **Storage Space (Input)**: Albedo/Base Color textures loaded from assets are sRGB encoded (gamma 2.2 approximate). Normal, Roughness, Metallic, AO, and Emissive textures are linear data and must not undergo sRGB conversion.
* **Shading Space (Computation)**: All lighting, BRDF computations, sky light irradiance, and specular pre-filtering must be performed in **Linear HDR space** to preserve physical scaling.
* **Storage Space (G-Buffer)**: The G-Buffer targets (`GBufferA` base color and `GBufferD` emissive) store linear data. Base color textures are sampled in the G-buffer pass and converted from sRGB to linear before being written to `GBufferA`.

## 2. Tonemapping and Presentation
* Lighting equations accumulate diffuse and specular radiance into `HDRColor` (`VK_FORMAT_R16G16B16A16_SFLOAT`).
* Tonemapping (e.g., ACES, Reinhard, or Uncharted) and gamma correction are applied at the end of the post-processing phase (`PostProcessPass`) when converting `HDRColor` to the viewport presentation targets.
* Material debug views (e.g. Albedo, Normal, Metallic, Roughness, AO preview) must bypass manual exposure and tonemapping, displaying raw linear G-buffer values directly to allow correct inspection of asset metadata.
