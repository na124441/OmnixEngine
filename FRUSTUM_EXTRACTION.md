# Camera Frustum Extraction and Culling System

This document outlines the implementation of the CPU-side frustum extraction, sphere culling, and GPU upload system added in Milestone 2.

## 1. Math and Theory

### Gribb-Hartmann Plane Extraction
The camera frustum is represented as six planes extracted from the view-projection matrix $M = P \times V$:
$$M = \begin{bmatrix} 
m_{00} & m_{10} & m_{20} & m_{30} \\
m_{01} & m_{11} & m_{21} & m_{31} \\
m_{02} & m_{12} & m_{22} & m_{32} \\
m_{03} & m_{13} & m_{23} & m_{33}
\end{bmatrix}$$

A point $v = (x, y, z, 1)^T$ in world space is inside the frustum if its clip-space coordinates satisfy:
$$-w_c \le x_c \le w_c$$
$$-w_c \le y_c \le w_c$$
$$-w_c \le z_c \le w_c \quad \text{(for OpenGL/homogeneous clip space)}$$

This yields six linear inequalities of the form $Ax + By + Cz + D \ge 0$. The coefficients $(A, B, C, D)$ form the plane equation $P = (A, B, C, D)^T$.

For example, the **Left Plane** equation is obtained from $x_c \ge -w_c \implies w_c + x_c \ge 0$:
$$P_{\text{Left}} = \text{row}_3(M) + \text{row}_0(M)$$

### Matrix Style: OpenGL vs Vulkan Clip Space
The standard Gribb-Hartmann extraction equations expect OpenGL clip space depth range $[-1, 1]$. Since Vulkan uses clip space depth range $[0, 1]$ and a flipped Y-axis, passing a Vulkan projection matrix directly to the extraction formula would result in incorrect plane equations (specifically for the Near, Far, and Top/Bottom planes). 

To ensure mathematical correctness without modifying the extraction formula, we build an OpenGL-compatible view-projection matrix using:
1. `glm::perspective` (which generates an OpenGL-style projection matrix).
2. The camera's pre-calculated `viewMatrix`.

This ensures that the extracted planes are aligned with the camera's visual frustum.

### Plane Normalization
To compute the exact world-space distance from a point to a plane, the plane normal $(A, B, C)$ must be normalized:
$$\text{length} = \sqrt{A^2 + B^2 + C^2}$$
$$P_{\text{Normalized}} = \frac{P}{\text{length}}$$

### Sphere Frustum Intersection
To determine if a bounding sphere with center $C$ and radius $R$ is completely outside the frustum:
1. For each of the 6 normalized planes $P = (A, B, C, D)^T$, calculate the signed distance to the sphere center:
   $$d = A \cdot C_x + B \cdot C_y + C \cdot C_z + D$$
2. If $d < -R$, the sphere is entirely on the outside of this plane, meaning it is invisible.
3. If $d \ge -R$ for all 6 planes, the sphere is at least partially inside the frustum (or intersects it) and is visible.

---

## 2. Code Structure and Files

### Data Types
* **`Rendering/GPUScene/GPUVisibilityTypes.h`**:
  Defines `struct GPUFrustum` containing the 6 planes aligned for GPU layout matching:
  ```cpp
  struct GPUFrustum {
      glm::vec4 planes[6];
  };
  ```

### CPU Frustum Extraction and Culling
* **`Rendering/Visibility/Frustum.h` & `Frustum.cpp`**:
  * `NormalizePlane`: Normalizes a plane vector.
  * `ExtractFrustumPlanes`: Extracts 6 frustum planes from a view-projection matrix.
  * `SphereInFrustumCPU`: Returns `true` if the sphere intersects or is inside the frustum.

### Integration
1. **CPU Culling Toggle**:
   * Added `m_CPUFrustumCulling` toggle in `Renderer.h` and `Renderer.cpp`.
   * Under `Renderer::buildRenderQueue()`, static objects and ECS renderable items are checked against the camera frustum on the CPU if culling is enabled.
2. **Editor View Menu**:
   * Added a menu item `View -> CPU Frustum Culling` in `EditorLayer.cpp` to toggle culling in real-time.
3. **GPU Uniform Buffer (`FrustumBuffer`)**:
   * Allocated a dedicated `frustumBuffer` in `GPUSceneFrameResources` inside `GPUScene.cpp`.
   * Added Binding 5 (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`) to Set 0's layout.
   * Bound the buffer inside `writeDescriptorSet` and updated it every frame inside `UpdateFrame` by extracting the active camera's frustum.

---

## 3. Verification & Log Outputs

On startup, the engine logs the normalized frustum planes once for sanity checks (ensuring no `NaN` values and correct normal lengths):

```text
[INFO] GPUScene: Initial Frustum Planes Extracted:
[INFO]   Plane Left: x=0.894427, y=0.000000, z=-0.447214, w=0.000000
[INFO]   Plane Right: x=-0.894427, y=0.000000, z=-0.447214, w=0.000000
[INFO]   Plane Bottom: x=0.000000, y=0.894427, z=-0.447214, w=0.000000
[INFO]   Plane Top: x=0.000000, y=-0.894427, z=-0.447214, w=0.000000
[INFO]   Plane Near: x=0.000000, y=0.000000, z=-1.000000, w=-0.100000
[INFO]   Plane Far: x=0.000000, y=0.000000, z=1.000000, w=1000.000000
```
