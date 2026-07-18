# RVG Resource Ownership

This document details the memory and lifecycle ownership boundaries.

## Ownership Rules
- **ECS MeshRendererComponent**: Owns only lightweight handles (`GeometryHandle`, `MaterialHandle`) and render flags. Absolutely no raw GPU resource pointers or allocations.
- **Asset Registries**: Own CPU asset metadata.
- **GeometryArena**: Owns globally addressable GPU geometry resources.
- **GPUScene**: Owns persistent GPU instance records.
- **Frame Graph**: Controls frame-local pass scheduling, transient targets, and layout transitions.
