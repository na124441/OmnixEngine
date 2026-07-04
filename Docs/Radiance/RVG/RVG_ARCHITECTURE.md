# Radiance Virtual Geometry (RVG) Architecture

This document describes the design of the GPU-driven clustered virtual geometry renderer.

## Pipeline & Architecture Map
```text
ECS MeshRendererComponent
    ↓
RenderScene extraction
    ↓
RenderItem generation
    ↓
Visibility system
    ↓
Depth/GBuffer passes
    ↓
Mesh buffer binding
    ↓
vkCmdDrawIndexed
```

## Core Components
1. **GeometryArena**: Shared memory buffers holding all vertex and index data.
2. **Cluster Renderer**: Processes clusters/meshlets of micro-triangles.
3. **Residency Manager**: Directs asynchronous loading/unloading of virtual geometry pages.
