# GPU Instance Bounds System

This document describes the design and implementation of the GPU Instance Bounds System (Milestone 1) in the OmnixEngine.

## Overview
To enable GPU-driven occlusion and frustum culling, every renderable instance must be associated with valid bounding volumes. In version v0.5, we implement **bounding spheres** for rendering instances.

Bounding spheres are:
- Compact to store (4 floats: center XYZ + radius W).
- Efficient to transform from local-space to world-space.
- Extremely cheap to test against view frustum planes.

---

## Data Structures

### Mesh Local Bounds
Each `Mesh` object stores its local-space AABB and Bounding Sphere in the `MeshBounds` structure:

```cpp
struct MeshBounds
{
    glm::vec3 localCenter = glm::vec3(0.0f);
    float localRadius = 1.0f;
    glm::vec3 localMin = glm::vec3(0.0f);
    glm::vec3 localMax = glm::vec3(0.0f);
};
```

### GPU Instance Layout
The `GPUInstance` struct matches the std430 storage buffer layout in the shaders, occupying exactly 160 bytes:

```cpp
struct GPUInstance
{
    glm::mat4 worldMatrix;          // 64 bytes
    glm::mat4 previousWorldMatrix;  // 64 bytes
    glm::vec4 boundsCenterRadius;   // 16 bytes (xyz = world-space center, w = world-space radius)
    uint32_t meshIndex;             // 4 bytes
    uint32_t materialIndex;         // 4 bytes
    uint32_t objectID;              // 4 bytes
    uint32_t flags;                 // 4 bytes
};
```

---

## Bounds Computation and Transformation

### 1. Local Bounds Computation (Mesh Load)
When vertices are parsed from a model source (e.g. Wavefront OBJ), the local bounds are calculated:

$$\vec{C}_{local} = \frac{\vec{P}_{min} + \vec{P}_{max}}{2}$$
$$R_{local} = \max_{i} \|\vec{P}_{i} - \vec{C}_{local}\|$$

### 2. World Bounds Transformation (GPUScene Upload)
On instance extraction and buffer update, local bounding spheres are transformed to world-space:

$$\vec{C}_{world} = \text{TransformPoint}(\mathbf{M}_{world}, \vec{C}_{local})$$
$$S_{max} = \max (\| \mathbf{M}_{col0} \|, \| \mathbf{M}_{col1} \|, \| \mathbf{M}_{col2} \|)$$
$$R_{world} = R_{local} \times S_{max}$$

Where $S_{max}$ is the maximum scale factor extracted from the transform matrix columns.

---

## Editor Visualization
A new editor option **"Show GPU Bounds"** is added under the **View** menu. When toggled, the engine draws wireframe bounding spheres using the `DebugDraw::DrawSphere` API.
