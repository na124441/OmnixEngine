# RVG Capability Tiers

This document describes the capability tiers detected at startup.

## Capability Tiers
- **Tier 0: CPU-Driven Indexed Rendering**: Standard fallback path binding per-mesh buffers.
- **Tier 1: GPU-Driven Indexed Indirect Rendering**: GPU frustum and occlusion culling via indirect draw commands.
- **Tier 2: Mesh Shader Cluster Rendering**: Clusters of triangles rendered using Task/Mesh shaders.
- **Tier 3: Software Micro-Triangle Rasterizer**: Ultra-fine triangles rasterized in compute.

## Required Features
- **Tier 1**: BDA, Descriptor Indexing, Atomics, Draw Indexed Indirect Count.
- **Tier 2**: Mesh shader extensions (`VK_EXT_mesh_shader` or `VK_NV_mesh_shader`).
- **Tier 3**: Large storage buffer limits, subgroup arithmetic, mesh shaders.
